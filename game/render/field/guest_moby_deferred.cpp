#include "render/field/guest_moby_deferred.h"

#include "emit_memory.h"
#include "gte_registers.h"
#include "render/field/guest_moby_frustum.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_word.h"
#include "render/field/guest_working_memory.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_screen.h"

#include <lucent/log.h>

#include <array>
#include <cstdlib>

namespace spyro::guest_moby_deferred {
namespace {

namespace gte = psx::gte;
namespace frustum = guest_moby_frustum;
using guest_word::asSigned;
using guest_word::sra;

// Each vertex's screen word (or its outcode) at kScratch + 4i, its depth kDepths on.
constexpr std::uint32_t kScratch = 0x1F800000u;
constexpr std::uint32_t kDepths = 0x1E0u;
constexpr std::uint32_t kVertexSlots = 0x100;
// The drawer's own scratchpad words (80047064..800470C8, 80047514, 8004773C).
constexpr std::uint32_t kCamera = kScratch + 0x3C0u; // five RT words
constexpr std::uint32_t kCloseCursor = kScratch + 0x3D4u;
constexpr std::uint32_t kCursor = kScratch + 0x3D8u;
constexpr std::uint32_t kTable = kScratch + 0x3DCu;
constexpr std::uint32_t kSine = kScratch + 0x3E0u;
constexpr std::uint32_t kBuckets = kScratch + 0x3E4u; // the walk's VXY1
constexpr std::uint32_t kShades = kScratch + 0x3E8u;  // where the near fade writes colours
constexpr std::uint32_t kBin = kScratch + 0x3ECu;     // the entry's table bin, in bytes
constexpr std::uint32_t kPalette = kScratch + 0x3F0u; // two pairs of colour words

// Above the walk's VXY1: the faded colours, then the buffer of 16-byte entries.
constexpr std::uint32_t kShadesAbove = 0x1600u;
constexpr std::uint32_t kEntriesAbove = 0x1C00u;
constexpr std::uint32_t kEntryBytes = 0x10u;
constexpr std::uint32_t kBucketBytes = 0xC00u;
constexpr std::uint32_t kBucketStride = 8;

// The light colour matrix, a fixed set of directions the flat faces are lit along (80047010).
constexpr std::array<std::uint32_t, 5> kLightColour = {
    0xF407F884u, 0xF884F400u, 0xF400F407u, 0xF407F884u, 0x0000F400u};
// A screen-placed mesh's rotation squashes y (8004728C..8004729C).
constexpr gte::RotationWords kScreenRotation = {0x1000u, 0, 0xA00u, 0, 0x1000u};
constexpr std::uint32_t kRetailOffsetX = 0x01000000u; // OFX 256, restored after each mesh
constexpr std::uint32_t kRetailOffsetY = 0x00780000u; // OFY 120

// The record's words this drawer reads beyond the walk's.
constexpr std::uint32_t kDetail = guest_moby::record::kDepthBias; // low byte: fade distance
constexpr std::uint32_t kPaletteIndex = guest_moby::record::kFlags;
constexpr std::uint32_t kCloseDepth = 0x1200u; // nearer than this joins the close list

// The mesh header at the slot's +0x14: two count words, two face lists and the vertex stream.
namespace mesh {
constexpr std::uint32_t kSlotHeader = 0x14;
constexpr std::uint32_t kCounts = 0x00; // vertex counts, the close list's tag in the top byte
constexpr std::uint32_t kBounds = 0x04; // four sphere margins, 8 bits each, << 4
constexpr std::uint32_t kFaded = 0x08;  // faces | colours << 16, and far colours at +0xE
constexpr std::uint32_t kPlain = 0x0C;  // the same for a mesh past the fade
constexpr std::uint32_t kVertices = 0x10;
} // namespace mesh

constexpr std::uint32_t kFadeStart = 0x1000u; // fade amount below this
constexpr std::uint32_t kFarFadeStart = 0x2000u;
constexpr std::uint32_t kFarFadeFull = 0x4000u;
constexpr std::uint32_t kFarFadeLimit = 0x1FFFu;
constexpr std::uint32_t kNearBias = 0x400u;
constexpr std::uint32_t kFarBin = 0x110u; // bins past this are drawn 0x100 bytes further
constexpr std::uint32_t kFarBinStep = 0x100u;
constexpr std::uint32_t kLastBin = 0xF00u;
constexpr std::uint32_t kGlowDepth = 0x800u;
constexpr std::uint32_t kGlowBright = 0x5C0u;

// GP0 tags and commands (80047918..80047E3C); a set bit 31 of a texture word makes it
// semi-transparent.
constexpr std::uint32_t kSemiTransparent = 0x02000000u;
struct Packet {
  std::uint32_t tag;
  std::uint32_t command;
  std::uint32_t bytes;
};
constexpr Packet kShadedQuad{0x08000000u, 0x38000000u, 0x24};
constexpr Packet kTexturedQuad{0x0C000000u, 0x3C000000u, 0x34};
constexpr Packet kShadedTriangle{0x06000000u, 0x30000000u, 0x1C};
constexpr Packet kTexturedTriangle{0x09000000u, 0x34000000u, 0x28};
constexpr Packet kFlatTriangle{0x04000000u, 0x22000000u, 0x14};
constexpr Packet kFlatQuad{0x05000000u, 0x2A000000u, 0x18};
// The draw-mode packet a palette mesh's chain starts with (80047FCC..80047FEC).
constexpr std::uint32_t kModeTag = 0x82000000u;
constexpr std::uint32_t kModeCommand = 0xE1000620u;

constexpr std::uint32_t kLow24 = 0x00FFFFFFu;

// A face word's two flag bits: 1 draws a triangle from either side (a flat face: picks its
// palette pair and nears its glow depth), 2 makes it a flat lit face.
constexpr std::uint32_t kTwoSided = 1;
constexpr std::uint32_t kFlat = 2;

// The four index fields of a face's first word and the four colour fields of its second.
constexpr std::uint32_t kIndexMask = 0x1FCu;
// A count field, in bytes of one word per vertex.
constexpr std::uint32_t kCountMask = 0x3FCu;

std::uint32_t channel(std::uint32_t value) {
  const std::uint32_t over = value - 0xFFFu;
  const std::uint32_t clamped = (asSigned(value) < 0 ? 0u : value) >> 4;
  return asSigned(over) >= 0 ? 0xFFu : clamped;
}

class DeferredDrawer {
public:
  DeferredDrawer(Core &core, const Image &image, Selection selection)
      : core_(core), memory_(core), image_(image), selection_(selection),
        slope_(presentedSlope(core)), bounds_(presentedBounds(core, image.terrain)) {}

  void draw();

private:
  struct Mesh {
    std::uint32_t header = 0;
    std::uint32_t counts = 0;
    std::uint32_t bounds = 0;
  };
  struct Translation {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
  };
  // A face's vertex slots, each a scratch address.
  struct Corners {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::uint32_t d = 0;
  };

  void drawEntry(std::uint32_t entry);
  void
  drawScreenPlaced(std::uint32_t rec, std::uint32_t offset, std::uint32_t z, std::uint32_t slot);
  void drawMesh(std::uint32_t rec,
                bool outcodes,
                const Translation &translation,
                const Mesh &mesh,
                gte::RotationWords rotation);
  // 80047514..80047614: false when every vertex lies past one edge.
  bool project(const Mesh &mesh, std::uint32_t fade, bool outcodes);
  // 80047618..8004770C and 80048018..800480C4: the colour table the faces read.
  void shade(const Mesh &mesh, std::uint32_t fade);
  void face();
  void quad(std::uint32_t second, const Corners &corners);
  void triangle(std::uint32_t second, const Corners &corners);
  void skipFace(std::uint32_t second);
  void
  shadedQuad(std::uint32_t second, const Corners &corners, std::uint32_t colour, std::int32_t bias);
  void shadedTriangle(std::uint32_t second,
                      const Corners &corners,
                      std::uint32_t colour,
                      std::int32_t bias,
                      bool fromQuad);
  // `screenB` and `screenC` are the second and third corners' screen words.
  void flatFace(std::uint32_t second,
                const Corners &corners,
                std::uint32_t facing,
                std::uint32_t screenB,
                std::uint32_t screenC);
  void link(std::uint32_t bucket, const Packet &packet);
  // 80047EA8..80048014: the local buckets into the ordering table.
  void merge();
  void mergeIntoOneBin(std::uint32_t bin);

  [[nodiscard]] std::uint32_t word(std::uint32_t address) const {
    return core_.mem_r32(address);
  }
  [[nodiscard]] std::uint32_t depth(std::uint32_t corner) const {
    return word(corner + kDepths);
  }
  [[nodiscard]] std::uint32_t colour(std::uint32_t second, std::uint32_t shift) const {
    return word(shades_ + ((second >> shift) & kIndexMask));
  }
  void putVertex(std::uint32_t offset, std::uint32_t corner, std::uint32_t screen) {
    memory_.storeVertex(cursor_ + offset,
                        gte::ProjectedVertex(screen, vertices_[(corner - kScratch) >> 2].place()));
  }
  void putWord(std::uint32_t offset, std::uint32_t value) {
    core_.mem_w32(cursor_ + offset, value);
  }
  void writeVertex(std::uint32_t packed) {
    core_.gte().writeData(gte::kVxy0, sra(packed << 10, 21) + (sra(packed << 20, 20) << 16));
    core_.gte().writeData(gte::kVz0, sra(packed, 21));
  }
  void writeFarColour(std::uint32_t packed) {
    core_.gte().writeControl(gte::kFarRed, (packed << 4) & 0xFF0u);
    core_.gte().writeControl(gte::kFarGreen, (packed >> 4) & 0xFF0u);
    core_.gte().writeControl(gte::kFarBlue, (packed >> 12) & 0xFF0u);
  }

  Core &core_;
  psx::present::EmitMemory memory_;
  const Image &image_;
  Selection selection_;
  frustum::HorizontalSlope slope_;
  guest_terrain::ScreenBounds bounds_;
  std::array<gte::ProjectedVertex, kVertexSlots> vertices_{};
  // One mesh's face walk: the next face and the list's end, the next face's first word, the
  // packet cursor, the buckets and the nearest and farthest bucket linked, the texture page step,
  // the depth less which faces bin, the colour table and whether vertices carry outcodes.
  std::uint32_t at_ = 0;
  std::uint32_t end_ = 0;
  std::uint32_t first_ = 0;
  std::uint32_t cursor_ = 0;
  std::uint32_t buckets_ = 0;
  std::uint32_t nearest_ = 0;
  std::uint32_t farthest_ = 0;
  std::uint32_t pageStep_ = 0;
  std::uint32_t near_ = 0;
  std::uint32_t shades_ = 0;
  bool outcodes_ = false;
  // Retail's v1 at return is what the last entry's path left in it; the O32 contract treats it
  // as a result, so the drawer carries the same value to the same exit.
  std::uint32_t v1_ = 0;
};

// 80046FD8..800470CC, 800480C8..80048124
void DeferredDrawer::draw() {
  const guest_render_globals::Globals &globals = image_.globals;
  for (std::uint32_t i = 0; i < kLightColour.size(); ++i) {
    core_.gte().writeControl(gte::kLightColour + i, kLightColour[i]);
  }
  const std::uint32_t scratch = core_.gte().readData(gte::kVxy1);
  core_.mem_w32(kCloseCursor, word(image_.walk.closeCursorHome));
  core_.mem_w32(kBuckets, scratch);
  core_.mem_w32(kShades, scratch + kShadesAbove);
  const gte::RotationWords camera = guest_memory::rotationAt(memory_, globals.cameraRotation);
  for (std::uint32_t i = 0; i < camera.size(); ++i) {
    core_.mem_w32(kCamera + 4 * i, camera[i]);
  }
  core_.mem_w32(kCursor, word(globals.primitiveCursor));
  core_.mem_w32(kTable, word(globals.orderingTable));
  core_.mem_w32(kSine, globals.cameraSineTable);
  v1_ = globals.cameraSineTable;
  for (std::uint32_t entry = scratch + kEntriesAbove; word(entry) != 0; entry += kEntryBytes) {
    drawEntry(entry);
  }
  core_.mem_w32(image_.walk.closeCursorHome, word(kCloseCursor));
  const std::uint32_t cursor = word(kCursor);
  core_.mem_w32(globals.primitiveCursor, cursor);
  core_.r[2] = cursor;
  core_.r[3] = v1_;
}

// 800470CC..80047254
void DeferredDrawer::drawEntry(std::uint32_t entry) {
  const std::uint32_t rec = word(entry);
  const std::uint32_t offset = word(entry + 4);
  const std::uint32_t reach = word(entry + 8);
  const std::uint32_t slot = word(entry + 12);
  const std::uint32_t z = sra(reach, 16);
  if (asSigned(reach << 16) < 0) {
    drawScreenPlaced(rec, offset, z, slot);
    return;
  }
  const gte::RotationWords camera = guest_memory::rotationAt(memory_, kCamera);
  core_.gte().loadRotation(camera);
  const std::uint32_t header = word(slot + mesh::kSlotHeader);
  core_.gte().writeData(gte::kIr3, sra(offset, 16));
  core_.gte().writeData(gte::kIr1, guest_word::signedHalf(offset));
  core_.gte().writeData(gte::kIr2, z);
  core_.gte().execute(gte::kMvmvaRtIr);
  const Mesh mesh{.header = header,
                  .counts = word(header + mesh::kCounts),
                  .bounds = word(header + mesh::kBounds)};
  const std::uint32_t viewX = core_.gte().readData(gte::kMac1);
  const std::uint32_t viewY = core_.gte().readData(gte::kMac2);
  const std::uint32_t viewZ = core_.gte().readData(gte::kMac3);
  v1_ = viewZ;
  if (asSigned(viewZ - (reach & 0xFFFFu)) >= 0) {
    return;
  }
  const frustum::ViewPoint view{asSigned(viewX), asSigned(viewY), asSigned(viewZ)};
  const frustum::SphereMargins margins{
      .horizontalLateral = static_cast<std::int32_t>((mesh.bounds >> 20) & 0xFF0u),
      .horizontalDepth = static_cast<std::int32_t>((mesh.bounds >> 12) & 0xFF0u),
      .verticalLateral = static_cast<std::int32_t>((mesh.bounds >> 4) & 0xFF0u),
      .verticalDepth = static_cast<std::int32_t>((mesh.bounds << 4) & 0xFF0u),
  };
  const bool outsideRetail = frustum::outsideHorizontal(view, margins, frustum::kRetailSlope);
  const bool drawn = selection_ == Selection::Retail
                         ? !outsideRetail
                         : outsideRetail && !frustum::outsideHorizontal(view, margins, slope_);
  if (!drawn || frustum::behindEye(view, static_cast<std::uint32_t>(margins.verticalLateral))) {
    return;
  }
  if (asSigned(word(rec + guest_moby::record::kCloseFlag)) < 0 &&
      asSigned(viewZ - kCloseDepth) < 0) {
    const std::uint32_t close = word(kCloseCursor);
    core_.mem_w32(close, rec);
    core_.mem_w32(close + 4, mesh.counts >> 24);
    core_.mem_w32(kCloseCursor, close + 8);
  }
  if (frustum::outsideVertical(view, margins)) {
    return;
  }
  if (selection_ == Selection::Retail) {
    core_.mem_w8(rec + guest_moby::record::kClass, 1);
  }
  drawMesh(rec,
           !frustum::insideFrustum(view, margins, slope_),
           {.x = viewX << 1, .y = viewY << 1, .z = viewZ << 1},
           mesh,
           camera);
}

// 80047258..800472B8: drawn at the entry's screen offset, never culled.
void DeferredDrawer::drawScreenPlaced(std::uint32_t rec,
                                      std::uint32_t offset,
                                      std::uint32_t z,
                                      std::uint32_t slot) {
  v1_ = z;
  if (asSigned(z) <= 0 || selection_ == Selection::Margin) {
    return;
  }
  core_.mem_w8(rec + guest_moby::record::kClass, 1);
  core_.gte().writeControl(gte::kOfx, offset & 0xFFFF0000u);
  core_.gte().writeControl(gte::kOfy, offset << 16);
  const std::uint32_t header = word(slot + mesh::kSlotHeader);
  const Mesh mesh{.header = header,
                  .counts = word(header + mesh::kCounts),
                  .bounds = word(header + mesh::kBounds)};
  core_.gte().loadRotation(kScreenRotation);
  drawMesh(rec, false, {.x = 0, .y = 0, .z = (z >> 2) << 1}, mesh, kScreenRotation);
}

// 800472BC..80047514
void DeferredDrawer::drawMesh(std::uint32_t rec,
                              bool outcodes,
                              const Translation &translation,
                              const Mesh &mesh,
                              gte::RotationWords rotation) {
  const std::uint32_t angles = word(rec + moby_record::kAngles);
  const std::uint32_t detail = core_.mem_r16(rec + kDetail);
  (void)guest_moby_rotation::composeTabled(core_, word(kSine), {.angles = angles}, rotation);
  core_.gte().writeControl(gte::kTranslationX, translation.x);
  core_.gte().writeControl(gte::kTranslationY, translation.y);
  core_.gte().writeControl(gte::kTranslationZ, translation.z);
  const std::uint32_t depth = translation.z << 1;
  const std::uint32_t fade = ((detail & 0xFFu) << 10) - depth;
  std::uint32_t bin = sra(depth, 7) - sra(angles, 24);
  if (asSigned(bin) < 0) {
    bin = 0;
  }
  near_ = translation.z - kNearBias;
  v1_ = translation.z;
  if (asSigned(bin - kFarBin) >= 0) {
    bin = (bin << 3) + kFarBinStep;
    if (asSigned(bin - kLastBin) >= 0) {
      core_.mem_w32(kBin, bin);
      return;
    }
  } else {
    bin <<= 3;
  }
  core_.mem_w32(kBin, bin);
  outcodes_ = outcodes;
  if (!project(mesh, fade, outcodes)) {
    nearest_ = mesh.counts;
    farthest_ = mesh.counts - kBucketStride;
    merge();
    return;
  }
  shade(mesh, fade);

  // 8004770C..80047784
  cursor_ = word(kCursor);
  if (const std::uint32_t palette = word(rec + kPaletteIndex); palette != 0) {
    shades_ &= 0x7FFFFFFFu;
    const std::uint32_t pair = image_.deferred.paletteTable + (palette << 4);
    for (std::uint32_t i = 0; i < 4; ++i) {
      core_.mem_w32(kPalette + 4 * i, word(pair + 4 * i));
    }
  }
  const std::uint32_t faceBytes = word(at_);
  near_ <<= 2;
  std::uint32_t skipped = 0;
  if (asSigned(near_) <= 0) {
    skipped = ((0u - near_) >> 5) << 3;
    near_ = 1;
  }
  buckets_ = word(kBuckets) + skipped;
  first_ = word(at_ + 4);
  nearest_ = buckets_ + kBucketBytes;
  farthest_ = buckets_;
  at_ += 4;
  end_ = at_ + faceBytes;
  while (at_ != end_) {
    face();
  }
  merge();
}

bool DeferredDrawer::project(const Mesh &mesh, std::uint32_t fade, bool outcodes) {
  std::uint32_t stream = mesh.header + mesh::kVertices;
  const std::uint32_t bytes =
      ((asSigned(fade) < 0 ? mesh.counts >> 14 : mesh.counts >> 6)) & kCountMask;
  if (bytes == 0) {
    lucent::error("mobydeferred", "mesh 0x{:08X} has no vertices", mesh.header);
    std::abort();
  }
  writeVertex(word(stream));
  stream += 4;
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::uint32_t slot = 0; slot != bytes; slot += 4) {
    core_.gte().execute(gte::kRtps);
    const std::uint32_t next = word(stream);
    stream += 4;
    const gte::ProjectedVertex screen = core_.gte().projected(2);
    const std::uint32_t screenDepth = core_.gte().readData(gte::kSz3);
    writeVertex(next);
    core_.mem_w32(kScratch + kDepths + slot, screenDepth);
    vertices_[slot >> 2] = screen;
    const std::uint32_t sxy = screen.word();
    if (!outcodes) {
      core_.mem_w32(kScratch + slot, sxy);
      continue;
    }
    std::uint32_t code = sxy << 5;
    if (asSigned(fade) <= 0) {
      code |= 0x10u;
    }
    if (bounds_.aboveTop(sxy)) {
      code |= 1u;
    }
    if (bounds_.belowBottom(sxy)) {
      code |= 2u;
    }
    if (bounds_.atOrLeftOfLeft(sxy)) {
      code |= 4u;
    }
    if (bounds_.atOrRightOfRight(sxy)) {
      code |= 8u;
    }
    shared &= code;
    core_.mem_w32(kScratch + slot, code);
  }
  return !outcodes || (shared & 0xFu) == 0;
}

void DeferredDrawer::shade(const Mesh &mesh, std::uint32_t fade) {
  const std::uint32_t header = mesh.header;
  const std::uint32_t colours = (mesh.counts << 2) & kCountMask;
  pageStep_ = 0;
  if (asSigned(fade) <= 0) {
    // 800476DC: past the fade; a far fade toward the level's colour while VZ1 is set.
    const std::uint32_t plain = word(header + mesh::kPlain);
    at_ = header + (plain & 0xFFFFu);
    shades_ = header + (plain >> 16);
    const std::uint32_t far = fade + kFarFadeStart;
    if (core_.gte().readData(gte::kVz1) == 0 || asSigned(far) >= 0) {
      return;
    }
    std::uint32_t amount = 0u - far;
    if (asSigned(far + kFarFadeStart) <= 0) {
      amount = kFarFadeLimit;
    }
    core_.gte().writeData(gte::kIr0, amount >> 1);
    std::uint32_t source = shades_;
    std::uint32_t out = core_.gte().readData(gte::kVxy1) + kShadesAbove;
    shades_ = out;
    writeFarColour(word(image_.deferred.fogColour));
    core_.gte().writeData(gte::kRgbc, word(source));
    const std::uint32_t end = source + colours;
    do {
      core_.gte().execute(gte::kDpcs);
      const std::uint32_t next = word(source + 4);
      source += 4;
      const std::uint32_t faded = core_.gte().readData(gte::kRgb2);
      core_.gte().writeData(gte::kRgbc, next);
      core_.mem_w32(out, faded);
      out += 4;
    } while (source != end);
    return;
  }
  const std::uint32_t faded = word(header + mesh::kFaded);
  at_ = header + (faded & 0xFFFFu);
  shades_ = header + (faded >> 16);
  if (asSigned(fade - kFadeStart) >= 0) {
    return;
  }
  // 8004762C: near, each vertex fades toward its own far colour by the distance left.
  const std::uint32_t amount = kFadeStart - fade;
  core_.gte().writeData(gte::kIr0, amount);
  std::uint32_t far = header + core_.mem_r16(header + mesh::kFaded + 6);
  std::uint32_t source = shades_;
  if (source == far) {
    return;
  }
  pageStep_ = (amount >> 9) << 22;
  std::uint32_t out = word(kShades);
  shades_ = out;
  const std::uint32_t end = far + colours;
  std::uint32_t farColour = word(far);
  std::uint32_t nearColour = word(source);
  do {
    core_.gte().writeData(gte::kRgbc, nearColour);
    writeFarColour(farColour);
    farColour = word(far + 4);
    source += 4;
    far += 4;
    core_.gte().execute(gte::kDpcs);
    nearColour = word(source);
    core_.mem_w32(out, core_.gte().readData(gte::kRgb2));
    out += 4;
  } while (far != end);
}

// 80047788..800477B8
void DeferredDrawer::face() {
  const std::uint32_t first = first_;
  const std::uint32_t second = word(at_ + 4);
  const Corners corners{.a = kScratch + ((first >> 19) & kIndexMask),
                        .b = kScratch + ((first >> 12) & kIndexMask),
                        .c = kScratch + ((first >> 5) & kIndexMask),
                        .d = kScratch + ((first << 2) & kIndexMask)};
  if (corners.c == corners.d) {
    triangle(second, corners);
  } else {
    quad(second, corners);
  }
}

void DeferredDrawer::skipFace(std::uint32_t second) {
  at_ += asSigned(second) < 0 ? 0x14u : 8u;
  first_ = word(at_);
}

// 800477C0..800478A8
void DeferredDrawer::quad(std::uint32_t second, const Corners &corners) {
  std::array<std::uint32_t, 4> screen = {
      word(corners.a), word(corners.b), word(corners.c), word(corners.d)};
  if (outcodes_) {
    if ((screen[0] & screen[1] & screen[2] & screen[3] & 0xFu) != 0) {
      skipFace(second);
      return;
    }
    for (std::uint32_t &corner : screen) {
      corner = sra(corner, 5);
    }
  }
  core_.gte().writeData(gte::kSxy0, screen[0]);
  core_.gte().writeData(gte::kSxy1, screen[1]);
  core_.gte().writeData(gte::kSxy2, screen[2]);
  core_.gte().execute(gte::kNclip);
  putVertex(8, corners.a, screen[0]);
  const bool textured = asSigned(second) < 0;
  if (textured) {
    putVertex(0x14, corners.b, screen[1]);
    putVertex(0x20, corners.c, screen[2]);
    putVertex(0x2C, corners.d, screen[3]);
    at_ += 0x14;
  } else {
    putVertex(0x10, corners.b, screen[1]);
    putVertex(0x18, corners.c, screen[2]);
    putVertex(0x20, corners.d, screen[3]);
    at_ += 8;
  }
  const std::uint32_t front = core_.gte().readData(gte::kMac0);
  core_.gte().writeData(gte::kSxy0, screen[3]);
  const std::int32_t bias = asSigned(first_) >> 28;
  core_.gte().execute(gte::kNclip);
  first_ = word(at_);
  const std::uint32_t back = core_.gte().readData(gte::kMac0);
  if ((second & kFlat) != 0) {
    // 80047C4C..80047C98
    putVertex(0x14, corners.d, screen[3]);
    if (asSigned(front) >= 0) {
      if (asSigned(back) > 0) {
        putVertex(8, corners.d, screen[3]);
        flatFace(second, {corners.d, corners.b, corners.c, corners.c}, front, screen[1], screen[2]);
      } else if (back != 0) {
        flatFace(second, corners, 0xFFFFFFFFu, screen[1], screen[2]);
      }
    } else if (asSigned(back) > 0) {
      flatFace(second, corners, 1, screen[1], screen[2]);
    } else {
      // Drawn as if front-facing: 80047C90 sets the facing to 1 on both branches.
      flatFace(second, {corners.a, corners.b, corners.c, corners.c}, 1, screen[1], screen[2]);
    }
    return;
  }
  if (asSigned(front) >= 0) {
    if (asSigned(back) <= 0) {
      return;
    }
    putVertex(8, corners.d, screen[3]);
    shadedTriangle(
        second, {corners.d, corners.b, corners.c, corners.c}, colour(second, 1), bias, true);
    return;
  }
  const std::uint32_t firstColour = colour(second, 22);
  if (asSigned(back) > 0) {
    shadedQuad(second, corners, firstColour, bias);
  } else {
    shadedTriangle(second, {corners.a, corners.b, corners.c, corners.c}, firstColour, bias, false);
  }
}

// 80047A2C..80047ABC, 80047C9C..80047CA8
void DeferredDrawer::triangle(std::uint32_t second, const Corners &corners) {
  std::array<std::uint32_t, 3> screen = {word(corners.a), word(corners.b), word(corners.c)};
  if (outcodes_) {
    if ((screen[0] & screen[1] & screen[2] & 0xFu) != 0) {
      skipFace(second);
      return;
    }
    for (std::uint32_t &corner : screen) {
      corner = sra(corner, 5);
    }
  }
  core_.gte().writeData(gte::kSxy0, screen[0]);
  core_.gte().writeData(gte::kSxy1, screen[1]);
  core_.gte().writeData(gte::kSxy2, screen[2]);
  const std::int32_t bias = asSigned(first_) >> 28;
  core_.gte().execute(gte::kNclip);
  putVertex(8, corners.a, screen[0]);
  if (asSigned(second) < 0) {
    putVertex(0x14, corners.b, screen[1]);
    putVertex(0x20, corners.c, screen[2]);
    at_ += 0x14;
  } else {
    putVertex(0x10, corners.b, screen[1]);
    putVertex(0x18, corners.c, screen[2]);
    at_ += 8;
  }
  const std::uint32_t facing = core_.gte().readData(gte::kMac0);
  first_ = word(at_);
  if (facing == 0) {
    return;
  }
  if ((second & kFlat) != 0) {
    flatFace(second, {corners.a, corners.b, corners.c, corners.c}, facing, screen[1], screen[2]);
    return;
  }
  if ((second & kTwoSided) == 0 && asSigned(facing) < 0) {
    return;
  }
  shadedTriangle(second, corners, colour(second, 22), bias, false);
}

// 800478AC..80047A28
void DeferredDrawer::shadedQuad(std::uint32_t second,
                                const Corners &corners,
                                std::uint32_t colour0,
                                std::int32_t bias) {
  const std::uint32_t colour1 = colour(second, 15);
  const std::uint32_t colour2 = colour(second, 8);
  const std::uint32_t colour3 = colour(second, 1);
  std::uint32_t sum =
      depth(corners.a) - near_ + depth(corners.b) + depth(corners.c) + depth(corners.d);
  const std::uint32_t biased = guest_word::asWord(bias) << 5;
  if (asSigned(second) < 0) {
    const std::uint32_t texture0 = word(at_ - 0xC);
    const std::uint32_t texture1 = word(at_ - 8);
    if (asSigned(sum) < 0) {
      return;
    }
    sum += biased;
    if (asSigned(sum) < 0) {
      return;
    }
    const std::uint32_t texture2 = word(at_ - 4);
    putWord(0x10, colour1);
    putWord(0x1C, colour2);
    putWord(0x28, colour3);
    putWord(4, (colour0 & kLow24) + kTexturedQuad.command + ((texture1 >> 31) * kSemiTransparent));
    putWord(0, kTexturedQuad.tag);
    putWord(0xC, texture0 + pageStep_);
    putWord(0x18, texture1);
    putWord(0x24, texture2);
    putWord(0x30, texture2 >> 16);
    link(buckets_ + (sra(sum, 5) << 3), kTexturedQuad);
    return;
  }
  putWord(0xC, colour1);
  putWord(0x14, colour2);
  putWord(0x1C, colour3);
  putWord(4, (colour0 & kLow24) + kShadedQuad.command);
  if (asSigned(sum) < 0) {
    return;
  }
  putWord(0, kShadedQuad.tag);
  sum += biased;
  if (asSigned(sum) < 0) {
    return;
  }
  link(buckets_ + (sra(sum, 5) << 3), kShadedQuad);
}

// 80047AD0..80047C34
void DeferredDrawer::shadedTriangle(std::uint32_t second,
                                    const Corners &corners,
                                    std::uint32_t colour0,
                                    std::int32_t bias,
                                    bool fromQuad) {
  const std::uint32_t colour1 = colour(second, 15);
  const std::uint32_t colour2 = colour(second, 8);
  std::uint32_t sum =
      depth(corners.a) - near_ + depth(corners.b) + depth(corners.b) + depth(corners.c);
  const std::uint32_t biased = guest_word::asWord(bias) << 5;
  if (asSigned(second) < 0) {
    const std::uint32_t texture0 = word(at_ - 0xC);
    const std::uint32_t texture1 = word(at_ - 8);
    const std::uint32_t texture2 = word(at_ - 4);
    if (asSigned(sum) < 0) {
      return;
    }
    sum += biased;
    if (asSigned(sum) < 0) {
      return;
    }
    putWord(0x10, colour1);
    putWord(0x1C, colour2);
    putWord(0, kTexturedTriangle.tag);
    putWord(4,
            ((colour0 & kLow24) | kTexturedTriangle.command) +
                ((texture1 >> 31) * kSemiTransparent));
    putWord(0xC, texture0 + pageStep_);
    putWord(0x18, texture1);
    putWord(0x24, texture2);
    if (fromQuad) {
      // The fourth corner's texture coordinates replace the first's.
      core_.mem_w16(cursor_ + 0xC, static_cast<std::uint16_t>(texture2 >> 16));
    }
    link(buckets_ + (sra(sum, 5) << 3), kTexturedTriangle);
    return;
  }
  putWord(0xC, colour1);
  putWord(0x14, colour2);
  putWord(4, (colour0 & kLow24) | kShadedTriangle.command);
  if (asSigned(sum) < 0) {
    return;
  }
  putWord(0, kShadedTriangle.tag);
  sum += biased;
  if (asSigned(sum) < 0) {
    return;
  }
  link(buckets_ + (sra(sum, 5) << 3), kShadedTriangle);
}

// 80047CAC..80047EA4: one flat colour lit along the face's normal, a glow while far.
void DeferredDrawer::flatFace(std::uint32_t second,
                              const Corners &corners,
                              std::uint32_t facing,
                              std::uint32_t screenB,
                              std::uint32_t screenC) {
  std::uint32_t semi = (facing >> 31) * kSemiTransparent;
  std::uint32_t glow = core_.gte().readControl(gte::kTranslationZ);
  const bool twoSided = (second & kTwoSided) != 0;
  if (twoSided) {
    glow -= kGlowDepth;
  }
  putVertex(0xC, corners.b, screenB);
  putVertex(0x10, corners.c, screenC);
  if (asSigned(glow) >= 0) {
    semi = kSemiTransparent;
    if (asSigned(facing) < 0) {
      return;
    }
  }
  core_.gte().writeData(gte::kIr3, sra(second << 1, 24));
  core_.gte().writeData(gte::kIr1, sra(second << 9, 24));
  core_.gte().writeData(gte::kIr2, sra(second << 17, 24));
  const std::uint32_t depthA = depth(corners.a);
  const std::uint32_t depthB = depth(corners.b);
  core_.gte().execute(gte::kMvmvaRtIr);
  const std::uint32_t depthC = depth(corners.c);
  const std::uint32_t depthD = depth(corners.d);
  std::uint32_t base = word(twoSided ? kPalette : kPalette + 8);
  const std::uint32_t light = word(twoSided ? kPalette + 4 : kPalette + 12);
  std::uint32_t sum = depthA - near_ + depthB + depthC + depthD;
  core_.gte().execute(gte::kMvmvaLcmIr);
  if (asSigned(sum) < 0) {
    return;
  }
  sum = sra(sum, 5);
  core_.gte().writeData(gte::kIr1, (light << 4) & 0xFF0u);
  core_.gte().writeData(gte::kIr2, (light >> 4) & 0xFF0u);
  core_.gte().writeData(gte::kIr3, (light >> 12) & 0xFF0u);
  std::uint32_t scale = sra(core_.gte().readData(gte::kMac1), 8);
  if (asSigned(facing) < 0) {
    scale = sra(scale, 2);
    base >>= 1;
    sum += 0x10;
  }
  core_.gte().writeData(gte::kIr0, scale);
  core_.gte().execute(gte::kGpf);
  std::uint32_t red = ((base << 4) & 0xFF0u) + sra(core_.gte().readData(gte::kMac1), 12);
  std::uint32_t green = ((base >> 4) & 0xFF0u) + sra(core_.gte().readData(gte::kMac2), 12);
  std::uint32_t blue = ((base >> 12) & 0xFF0u) + sra(core_.gte().readData(gte::kMac3), 12);
  if (const std::uint32_t bright = scale - kGlowBright; asSigned(bright) > 0) {
    red += bright << 3;
    green += bright << 3;
    blue += bright << 3;
  }
  const Packet &packet = corners.c == corners.d ? kFlatTriangle : kFlatQuad;
  putWord(0, packet.tag);
  putWord(4, packet.command - semi + channel(red) + (channel(green) << 8) + (channel(blue) << 16));
  link(buckets_ + (sum << 3), packet);
}

void DeferredDrawer::link(std::uint32_t bucket, const Packet &packet) {
  if (depth_bins::linkIntoBin(memory_, bucket, cursor_, cursor_) == 0) {
    if (asSigned(nearest_ - bucket) > 0) {
      nearest_ = bucket;
    }
    if (asSigned(farthest_ - bucket) < 0) {
      farthest_ = bucket;
    }
  }
  cursor_ += packet.bytes;
}

void DeferredDrawer::merge() {
  core_.gte().writeControl(gte::kOfx, kRetailOffsetX);
  core_.gte().writeControl(gte::kOfy, kRetailOffsetY);
  if (asSigned(farthest_ - nearest_) < 0) {
    v1_ = word(kBuckets);
    return;
  }
  const std::uint32_t bin = word(kBin);
  if (asSigned(shades_) > 0) {
    mergeIntoOneBin(bin);
    return;
  }
  // 80047ECC..80047F88: eight buckets to a table bin, from the farthest down.
  core_.mem_w32(kCursor, cursor_);
  v1_ = sra(word(kBuckets) + 0x400u - farthest_, 6) << 3;
  std::uint32_t slot = bin - v1_ - depth_bins::kBinStride;
  if (asSigned(slot) < 0) {
    slot = 0;
  }
  const std::uint32_t table = word(kTable);
  const std::uint32_t last = nearest_ - kBucketStride;
  std::uint32_t bucket = farthest_;
  slot += table;
  for (;;) {
    std::uint32_t tail = word(slot);
    std::uint32_t stop = bucket - 8 * kBucketStride;
    if (asSigned(last - stop) > 0) {
      stop = last;
    }
    bool chained = tail != 0;
    if (!chained) {
      while (bucket != stop) {
        const std::uint32_t head = word(bucket + 4);
        bucket -= kBucketStride;
        if (head != 0) {
          tail = word(bucket + 8);
          core_.mem_w32(slot + depth_bins::kBinFirstOffset, head);
          core_.mem_w32(bucket + 0xC, 0);
          core_.mem_w32(bucket + 8, 0);
          chained = true;
          break;
        }
      }
    }
    if (chained) {
      while (bucket != stop) {
        const std::uint32_t head = word(bucket + 4);
        bucket -= kBucketStride;
        if (head == 0) {
          continue;
        }
        const std::uint32_t next = word(bucket + 8);
        guest_memory::linkAfter(memory_, tail, head);
        tail = next;
        core_.mem_w32(bucket + 8, 0);
        core_.mem_w32(bucket + 0xC, 0);
      }
    }
    core_.mem_w32(slot, tail);
    if (stop == last) {
      return;
    }
    if (slot != table) {
      slot -= depth_bins::kBinStride;
    }
  }
}

// 80047F8C..80048014: a palette mesh's buckets chained far to near after one draw-mode packet,
// all in the entry's bin.
void DeferredDrawer::mergeIntoOneBin(std::uint32_t bin) {
  std::uint32_t tail = word(farthest_);
  const std::uint32_t head = word(farthest_ + 4);
  core_.mem_w32(farthest_, 0);
  core_.mem_w32(farthest_ + 4, 0);
  std::uint32_t bucket = farthest_;
  while (bucket != nearest_) {
    const std::uint32_t next = word(bucket - 4);
    bucket -= kBucketStride;
    if (next == 0) {
      continue;
    }
    guest_memory::linkAfter(memory_, tail, next);
    tail = word(bucket);
    core_.mem_w32(bucket, 0);
    core_.mem_w32(bucket + 4, 0);
  }
  const std::uint32_t slot = word(kTable) + bin;
  putWord(0, kModeTag ^ head);
  putWord(4, kModeCommand);
  putWord(8, 0);
  core_.mem_w32(kCursor, cursor_ + 0xC);
  (void)depth_bins::linkIntoBin(memory_, slot, cursor_, tail);
  v1_ = head;
}

} // namespace

void draw(Core &core, const Image &image, Selection selection) {
  guest_render_globals::spillBorrowedRegisters(core, image.globals);
  DeferredDrawer(core, image, selection).draw();
}

} // namespace spyro::guest_moby_deferred
