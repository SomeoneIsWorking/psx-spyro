#include "render/field/guest_moby_shadows.h"

#include "emit_memory.h"
#include "gte_registers.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_word.h"
#include "render/field/guest_working_memory.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_screen.h"

#include <array>

namespace spyro::guest_moby_shadows {
namespace {

namespace gte = psx::gte;
using guest_word::asSigned;

// The prologue spills s0..s7, gp, sp, fp, ra here (800499D4..80049A04).
constexpr std::uint32_t kSaveArea = 0x1F8003D0u;
// The four corners' screen words, then their depths, each with the first repeated after the last.
constexpr std::uint32_t kCorners = 0x1F800000u;
constexpr std::uint32_t kDepths = 0x1F800020u;
constexpr std::uint32_t kCornerCount = 4;
// The close list starts this far above the drawer scratch the walk parks in VXY1 (80049A44).
constexpr std::uint32_t kListAboveScratch = 0x1400u;
constexpr std::uint32_t kListEntryBytes = 8;
// The edge corners' texture word is the centre's plus this (80049E4C); the image stores the
// difference less it (80049A5C).
constexpr std::uint32_t kEdgeTextureStep = 0x1F00u;

// The record's floor word: its height in the low halfword, slope angles in 6-bit fields above.
constexpr std::uint32_t kFloor = 0x1C;
constexpr std::uint32_t kSlopeAngle1Shift = 16;
constexpr std::uint32_t kSlopeAngle0Shift = 22;
constexpr std::uint32_t kSlopeAngleMask = 0x3Fu;
constexpr std::uint32_t kSlopeAngleStep = 4; // a 6-bit angle indexes every fourth table entry
constexpr std::uint32_t kBias = 0x47;        // i8: bins nearer by this less kBiasBase
constexpr std::int32_t kBiasBase = 3;

constexpr std::uint32_t kNearLimit = 0x1000u; // view depth at which no shadow is drawn
constexpr std::uint32_t kFadeDepth = 0xC00u;  // the shade fades past this depth
constexpr std::uint32_t kFadeEnd = 0x400u;
constexpr std::uint32_t kFadeShift = 3;
constexpr std::uint32_t kFullShade = 0x80u;
constexpr std::uint32_t kNudgeDepth = 0x400u; // past it the nearest corner moves down a row
constexpr std::uint32_t kRow = 0x10000u;
constexpr std::uint32_t kBinShift = 7;
constexpr std::uint32_t kBottom = 0x00E40000u; // `lui 0xE4` at 80049D44: row 228

// POLY_FT3, semi-transparent, and its 7-word tag.
constexpr std::uint32_t kCommand = 0x26000000u;
constexpr std::uint32_t kTag = 0x07000000u;
constexpr std::uint32_t kGreyStep = 0x10101u;
constexpr std::uint32_t kPacketBytes = 0x20u;
namespace packet {
constexpr std::uint32_t kCommand = 0x04;
constexpr std::uint32_t kCentre = 0x08;
constexpr std::uint32_t kCentreTexture = 0x0C;
constexpr std::uint32_t kFirst = 0x10;
constexpr std::uint32_t kFirstTexture = 0x14;
constexpr std::uint32_t kSecond = 0x18;
constexpr std::uint32_t kSecondTexture = 0x1C;
} // namespace packet

class ShadowDrawer {
public:
  ShadowDrawer(Core &core, const Image &image)
      : core_(core), memory_(core), image_(image), bounds_(presentedBounds(core, image.terrain)) {}

  void draw();

private:
  void drawEntry(std::uint32_t entry);
  // 80049DB0..80049E10: the nearest corner moves down a row and the farthest up one.
  void nudgeCorners();
  // 80049E24..80049EB0: one triangle per edge, fanned from the centre.
  void fan(const gte::ProjectedVertex &centre,
           std::uint32_t command,
           std::uint32_t centreDepth,
           std::int32_t bias);

  Core &core_;
  psx::present::EmitMemory memory_;
  const Image &image_;
  guest_terrain::ScreenBounds bounds_;
  gte::RotationWords camera_{};
  std::uint32_t cameraX_ = 0;
  std::uint32_t cameraY_ = 0;
  std::uint32_t cameraZ_ = 0;
  std::uint32_t centreTexture_ = 0;
  std::uint32_t edgeTexture_ = 0;
  std::uint32_t cursor_ = 0;
  std::uint32_t table_ = 0;
  // Retail's v0 and v1 at return are what the last entry's path left in them; the O32 contract
  // treats both as results, so the drawer carries the same values to the same exit.
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
};

// 80049A08..80049A74
void ShadowDrawer::draw() {
  const guest_render_globals::Globals &globals = image_.globals;
  camera_ = guest_memory::rotationAt(memory_, globals.cameraRotation);
  cameraX_ = core_.mem_r32(globals.cameraPosition) >> guest_render_globals::kNearViewShift;
  cameraY_ = core_.mem_r32(globals.cameraPosition + 4) >> guest_render_globals::kNearViewShift;
  cameraZ_ = core_.mem_r32(globals.cameraPosition + 8) >> guest_render_globals::kNearViewShift;
  const std::uint32_t list = core_.gte().readData(gte::kVxy1) + kListAboveScratch;
  const std::uint32_t texture = image_.shadows.texture;
  centreTexture_ = core_.mem_r32(texture);
  edgeTexture_ = core_.mem_r32(texture + 4) - kEdgeTextureStep;
  const std::uint32_t end = core_.mem_r32(texture + 8);
  cursor_ = core_.mem_r32(globals.primitiveCursor);
  table_ = core_.mem_r32(globals.orderingTable);
  v0_ = core_.r[2];
  v1_ = core_.r[3];
  for (std::uint32_t entry = list; entry != end; entry += kListEntryBytes) {
    drawEntry(entry);
  }
  core_.mem_w32(globals.primitiveCursor, cursor_);
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

// 80049A80..80049E20
void ShadowDrawer::drawEntry(std::uint32_t entry) {
  const std::uint32_t rec = core_.mem_r32(entry);
  core_.gte().loadRotation(camera_);
  core_.gte().writeControl(gte::kTranslationX, 0);
  core_.gte().writeControl(gte::kTranslationY, 0);
  core_.gte().writeControl(gte::kTranslationZ, 0);
  const std::uint32_t floor = core_.mem_r32(rec + kFloor);
  const std::uint32_t height = floor & 0xFFFFu;
  v1_ = core_.mem_r32(rec + moby_record::kX);
  if (height == 0) {
    return;
  }
  // The walk's axes: x from camera y less the record's, y from the camera's height less the floor,
  // z from the record's x less the camera's.
  core_.gte().writeData(gte::kVxy0,
                        ((cameraZ_ - height) << 16) +
                            (cameraY_ - (core_.mem_r32(rec + moby_record::kY) >> 2)));
  core_.gte().writeData(gte::kVz0, (v1_ >> 2) - cameraX_);
  core_.gte().execute(gte::kRtps);
  const std::int32_t bias = core_.mem_r8s(rec + kBias) - kBiasBase;
  const std::uint32_t viewZ = core_.gte().readData(gte::kMac3);
  const std::uint32_t viewY = core_.gte().readData(gte::kMac2);
  const std::uint32_t viewX = core_.gte().readData(gte::kMac1);
  core_.gte().writeControl(gte::kTranslationZ, viewZ);
  v0_ = ((floor >> kSlopeAngle1Shift) & kSlopeAngleMask) << 3;
  v1_ = image_.globals.cameraSineTable;
  if (asSigned(viewZ - kNearLimit) >= 0) {
    return;
  }
  core_.gte().writeControl(gte::kTranslationY, viewY);
  core_.gte().writeControl(gte::kTranslationX, viewX);
  gte::RotationWords rotation = camera_;
  const std::uint32_t angle1 = ((floor >> kSlopeAngle1Shift) & kSlopeAngleMask) * kSlopeAngleStep;
  const std::uint32_t angle0 = ((floor >> kSlopeAngle0Shift) & kSlopeAngleMask) * kSlopeAngleStep;
  (void)guest_moby_rotation::composeTabled(
      core_, image_.globals.cameraSineTable, {.angles = (angle1 << 8) | angle0}, rotation);
  core_.gte().loadRotation(rotation);

  // 80049C70..80049CA0: three corners of the square, half a side `half` from its centre.
  const std::uint32_t half = core_.mem_r32(entry + 4) << 1;
  const std::uint32_t minusHalf = (0u - half) & 0xFFFFu;
  core_.gte().writeData(gte::kVxy0, 0);
  core_.gte().writeData(gte::kVz0, half);
  core_.gte().writeData(gte::kVxy1, half);
  core_.gte().writeData(gte::kVz1, 0);
  core_.gte().writeData(gte::kVxy2, 0);
  core_.gte().writeData(gte::kVz2, minusHalf);
  const gte::ProjectedVertex centre = core_.gte().projected(2);
  const std::uint32_t centreDepth = core_.gte().readData(gte::kSz3);
  core_.gte().execute(gte::kRtpt);
  std::uint32_t shade = kFullShade;
  if (const std::uint32_t past = centreDepth - kFadeDepth; asSigned(past) > 0) {
    shade = (kFadeEnd - past) >> kFadeShift;
  }
  const std::uint32_t command = kCommand + shade * kGreyStep;
  std::array<gte::ProjectedVertex, kCornerCount> corners{
      core_.gte().projected(0), core_.gte().projected(1), core_.gte().projected(2), {}};
  core_.gte().execute(gte::kNclip);
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.storeVertex(kCorners + 4 * i, corners[i]);
  }
  const std::uint32_t depth1 = core_.gte().readData(gte::kSz1);
  const std::uint32_t depth2 = core_.gte().readData(gte::kSz2);
  const std::uint32_t depth3 = core_.gte().readData(gte::kSz3);
  const std::uint32_t facing = core_.gte().readData(gte::kMac0);
  core_.gte().writeData(gte::kVxy0, minusHalf);
  core_.gte().writeData(gte::kVz0, 0);
  core_.mem_w32(kDepths, depth1);
  v0_ = facing;
  v1_ = depth1;
  if (asSigned(facing) < 0) {
    return;
  }
  core_.mem_w32(kDepths + 4, depth2);
  core_.mem_w32(kDepths + 8, depth3);
  core_.gte().execute(gte::kRtps);
  corners[3] = core_.gte().projected(2);
  memory_.storeVertex(kCorners + 0xC, corners[3]);
  core_.mem_w32(kDepths + 0xC, core_.gte().readData(gte::kSz3));

  // 80049D34..80049DA8: all above row 0, the last three at or below row 228, the last three left
  // of the window, or all at or right of it.
  const std::uint32_t w0 = corners[0].word();
  const std::uint32_t w1 = corners[1].word();
  const std::uint32_t w2 = corners[2].word();
  const std::uint32_t w3 = corners[3].word();
  if (asSigned(w0 & w1 & w2 & w3) < 0) {
    return;
  }
  const std::uint32_t below = kBottom | (w1 - kBottom) | (w2 - kBottom) | (w3 - kBottom);
  v0_ = w1 - kBottom;
  v1_ = w2 - kBottom;
  if (asSigned(below) >= 0) {
    return;
  }
  v0_ = w1 << 16;
  v1_ = w2 << 16;
  if (asSigned(below & bounds_.fromLeft(w1) & bounds_.fromLeft(w2) & bounds_.fromLeft(w3)) < 0) {
    return;
  }
  v0_ = bounds_.fromRight(w1);
  v1_ = bounds_.fromRight(w2);
  if (asSigned(bounds_.fromRight(w0) | v0_ | v1_ | bounds_.fromRight(w3)) > 0) {
    return;
  }
  v0_ = kCorners + 4 * kCornerCount;
  if (asSigned(centreDepth - kNudgeDepth) > 0) {
    nudgeCorners();
  }
  memory_.storeVertex(kCorners + 4 * kCornerCount, memory_.loadVertex(kCorners));
  core_.mem_w32(kDepths + 4 * kCornerCount, core_.mem_r32(kDepths));
  fan(centre, command, centreDepth, bias);
}

void ShadowDrawer::nudgeCorners() {
  std::uint32_t nearest = 0;
  std::uint32_t farthest = 0;
  for (std::uint32_t i = 1; i < kCornerCount; ++i) {
    const std::uint32_t depth = core_.mem_r32(kDepths + 4 * i);
    if (asSigned(depth - core_.mem_r32(kDepths + 4 * nearest)) < 0) {
      nearest = i;
    }
    if (asSigned(depth - core_.mem_r32(kDepths + 4 * farthest)) > 0) {
      farthest = i;
    }
  }
  const gte::ProjectedVertex down = memory_.loadVertex(kCorners + 4 * nearest);
  const gte::ProjectedVertex up = memory_.loadVertex(kCorners + 4 * farthest);
  memory_.storeVertex(kCorners + 4 * nearest, down.steppedTo(down.word() + kRow));
  memory_.storeVertex(kCorners + 4 * farthest, up.steppedTo(up.word() - kRow));
}

void ShadowDrawer::fan(const gte::ProjectedVertex &centre,
                       std::uint32_t command,
                       std::uint32_t centreDepth,
                       std::int32_t bias) {
  for (std::uint32_t i = 0; i < kCornerCount; ++i) {
    const gte::ProjectedVertex first = memory_.loadVertex(kCorners + 4 * i);
    const gte::ProjectedVertex second = memory_.loadVertex(kCorners + 4 * i + 4);
    const std::uint32_t depths =
        core_.mem_r32(kDepths + 4 * i) + core_.mem_r32(kDepths + 4 * i + 4);
    core_.mem_w32(cursor_ + packet::kCommand, command);
    memory_.storeVertex(cursor_ + packet::kCentre, centre);
    core_.mem_w32(cursor_ + packet::kCentreTexture, centreTexture_);
    core_.mem_w32(cursor_ + packet::kFirstTexture, edgeTexture_);
    core_.mem_w32(cursor_ + packet::kSecondTexture, centreTexture_ + kEdgeTextureStep);
    memory_.storeVertex(cursor_ + packet::kFirst, first);
    memory_.storeVertex(cursor_ + packet::kSecond, second);
    core_.mem_w32(cursor_, kTag);
    const std::uint32_t bin =
        ((depths + (depths >> 1) + centreDepth) >> kBinShift) - guest_word::asWord(bias);
    v1_ = bin * depth_bins::kBinStride;
    if (asSigned(bin) < 0) {
      continue;
    }
    v1_ += table_;
    (void)depth_bins::linkIntoBin(memory_, v1_, cursor_, cursor_);
    cursor_ += kPacketBytes;
  }
}

} // namespace

void draw(Core &core, const Image &image) {
  guest_render_globals::spillBorrowedRegisters(core, kSaveArea);
  ShadowDrawer(core, image).draw();
}

} // namespace spyro::guest_moby_shadows
