// guest_terrain_far.cpp — the far pass of the terrain drawer of this engine family (SCUS_944.25
// 80028B14): every sector the classification put on kFarList, drawn from its low-detail mesh as
// untextured Gouraud polygons (G3 / G4).
//
// A far sector's low-detail mesh is a packed vertex array, a colour array and 8-byte polygon
// records. Its vertices are projected into the scratchpad as (screen word, SZ) pairs; for a sector
// the classification found touching a screen edge or the near plane, the screen word is SXY << 5
// with outcode bits below it (1 above, 2 below, 4 outside the window horizontally, 8 nearer than
// the sector's near limit), and a sector whose vertices all share one is skipped whole. Each
// polygon is backface-tested unless two-sided, sorted by its mean depth, and dropped past the
// level's far bin. The pass stops when the primitive buffer reaches its end, the one place the
// drawer runs out of room.
//
// It also tracks the deepest ordering-table bin it starts (the mark at kOrderingTableMark), which
// the drawer stores on exit with the primitive cursor.

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_far_vertices.h"
#include "render/terrain/guest_terrain_fog.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/guest_terrain_passes.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "render/terrain/guest_terrain_screen.h"

#include <array>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;
using spyro::guest_word::sra;

// SCUS_944.25 80028B2C: the buffer ends at scratch + 0x1000.

// The far sector header (the near passes read others, guest_terrain_mesh.h).
namespace far_sector {
inline constexpr std::uint32_t kOrigin = 0x08;  // z origin high, x origin low
inline constexpr std::uint32_t kOriginY = 0x0E; // halfword
inline constexpr std::uint32_t kLayout = 0x10;  // see FarLayout
inline constexpr std::uint32_t kData = 0x1C;
} // namespace far_sector

// The layout word: vertex count, colour bytes, record bytes, the near limit, and fog exemption.
struct FarLayout {
  explicit FarLayout(std::uint32_t word)
      : vertexCount(word & 0xFFu), colourBytes((word >> 6) & 0x3FCu),
        recordBytes((word >> 13) & 0x7F8u), nearBias(((word >> 15) & 0x600u) - 0x760u),
        fogExempt(asSigned(word) < 0) {}

  std::uint32_t vertexCount;
  std::uint32_t colourBytes;
  std::uint32_t recordBytes;
  std::uint32_t nearBias; // SZ + nearBias < 0: nearer than the low-detail mesh is meant for
  bool fogExempt;
};

constexpr std::uint32_t kRecordSize = 8;
constexpr std::uint32_t kTwoSided = 0x80000000u; // in the record's vertex word
constexpr std::uint32_t kBinBase = 0x30;
constexpr std::uint32_t kBinBiasMask = 0xF;

constexpr std::uint32_t kOutcodeMask = 0xF;


enum class Sector : std::uint8_t { Drawn, BufferFull };

class FarPass {
public:
  explicit FarPass(TerrainFrame &frame) : frame_(frame), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  Sector drawSector(std::uint32_t listWord);
  Sector drawPolygon(std::uint32_t record);
  bool link(std::uint32_t bin, std::uint32_t bytes);

  TerrainFrame &frame_;
  Core &core_;
  const psx::present::EmitMemory &memory_;
  std::uint32_t lastBin_ = 0;   // s2
  std::uint32_t bufferEnd_ = 0; // LO
  // s3: the deepest bin any primitive has started in, as an ORDINAL. Retail holds the bin's slot
  // address and compares addresses; the ordinal is the same ordering with no address in it, which
  // is what lets this comparison be the same code over a table the guest owns and over one an
  // in-between field owns in host memory.
  std::uint32_t markBin_ = 0;
  std::array<std::uint32_t, 3> camera_{}; // s7, t8, t9
  std::array<std::uint32_t, 3> origin_{}; // s4, s5, s6
  // The sector being drawn: its list class (0 inside, else clipped; bit 0 an edge) and the colour
  // table its records index.
  std::uint32_t sectorClass_ = 0;
  std::uint32_t colours_ = 0;
};

// 80028B14.
void FarPass::run() {
  lastBin_ = (memory_.mem_r32(frame_.facts.far.farDepth) >> 7) - 1;
  bufferEnd_ = frame_.primitiveLimit();
  markBin_ = frame_.markBinFrom(memory_.mem_r32(frame_.globals.orderingTableMark));
  for (std::uint32_t i = 0; i < camera_.size(); ++i) {
    camera_[i] = frame_.positionWord(i) >> 4;
  }
  std::uint32_t list = frame_.scratch + kFarList;
  for (;;) {
    const std::uint32_t word = memory_.mem_r32(list);
    list += 4;
    if (word == 0 || drawSector(word) == Sector::BufferFull) {
      break;
    }
  }
  // 80029118: the drawer's exit stores the mark in a guest word the next frame's draw starts from,
  // so only the guest's own field publishes it.
  if (frame_.realField()) {
    core_.mem_w32(frame_.globals.orderingTableMark, frame_.markSlot(markBin_));
  }
}

// 80028B80.
Sector FarPass::drawSector(std::uint32_t listWord) {
  sectorClass_ = listWord & 3u;
  const std::uint32_t sector = listWord ^ sectorClass_;
  const std::uint32_t originWord = memory_.mem_r32(sector + far_sector::kOrigin);
  origin_ = {(originWord >> 16) - camera_[0],
             camera_[1] - (originWord & 0xFFFFu),
             camera_[2] - memory_.mem_r16(sector + far_sector::kOriginY)};
  const FarLayout layout(memory_.mem_r32(sector + far_sector::kLayout));
  const std::uint32_t data = sector + far_sector::kData;
  const FarClip clip{
      .clipped = sectorClass_ != 0, .edge = (sectorClass_ & 1u) != 0, .nearBias = layout.nearBias};
  if (!projectFarVertices(frame_,
                          {.data = data, .count = layout.vertexCount, .origin = origin_},
                          clip,
                          FarHeight::Doubled)) {
    return Sector::Drawn;
  }

  // 80028D8C: the colours, fogged when the level fogs and the sector allows it.
  std::uint32_t colours = data + 4 * layout.vertexCount;
  const std::uint32_t records = colours + layout.colourBytes;
  const std::uint32_t fog = core_.gte().readControl(gte::kLight3);
  if (fog != 0 && !layout.fogExempt) {
    frame_.v0 = records + 4;
    frame_.v1 = fogColours(memory_, core_, frame_.facts, colours, records + 4, fog);
    colours = frame_.facts.foggedColours;
  }
  colours_ = colours;
  const std::uint32_t recordsEnd = records + layout.recordBytes;
  for (std::uint32_t record = records; record != recordsEnd; record += kRecordSize) {
    if (drawPolygon(record) == Sector::BufferFull) {
      return Sector::BufferFull;
    }
  }
  return Sector::Drawn;
}

// 80028E28: one record, a G4 or (third and fourth vertex equal) a G3.
Sector FarPass::drawPolygon(std::uint32_t record) {
  const std::uint32_t colours = colours_;
  const bool clipped = sectorClass_ != 0;
  const std::uint32_t vertices = memory_.mem_r32(record);
  const std::uint32_t colourWord = memory_.mem_r32(record + 4);
  const std::array<std::uint32_t, 4> cells = {kScratchpad + ((vertices >> 21) & 0x3F8u),
                                              kScratchpad + ((vertices >> 14) & 0x3F8u),
                                              kScratchpad + ((vertices >> 7) & 0x3F8u),
                                              kScratchpad + (vertices & 0x3F8u)};
  const bool triangle = cells[2] == cells[3];
  const std::uint32_t corners = triangle ? 3 : 4;
  std::array<std::uint32_t, 4> screens{};
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::uint32_t i = 0; i < corners; ++i) {
    screens[i] = memory_.mem_r32(cells[i]);
    shared &= screens[i];
  }
  frame_.v0 = screens[1];
  frame_.v1 = screens[2];
  if (clipped) {
    if ((shared & kOutcodeMask) != 0) {
      return Sector::Drawn;
    }
    for (std::uint32_t i = 0; i < corners; ++i) {
      screens[i] = sra(screens[i], kFarOutcodeShift);
    }
    frame_.v0 = screens[1];
    frame_.v1 = screens[2];
  }
  core_.gte().writeData(gte::kSxy0, screens[0]);
  core_.gte().writeData(gte::kSxy1, screens[1]);
  core_.gte().writeData(gte::kSxy2, screens[2]);
  core_.gte().execute(gte::kNclip);
  std::uint32_t depth = 0;
  for (std::uint32_t i = 0; i < corners; ++i) {
    depth += memory_.mem_r32(cells[i] + 4);
  }
  const bool twoSided = (vertices & kTwoSided) != 0;
  std::uint32_t bin = 0;
  if (triangle) {
    // 80029048: one-sided triangles facing away are dropped.
    if (!twoSided && asSigned(core_.gte().readData(gte::kMac0)) <= 0) {
      return Sector::Drawn;
    }
    depth += memory_.mem_r32(cells[2] + 4);
    bin = (depth >> 5) + (colourWord & kBinBiasMask) + kBinBase;
  } else {
    // 80028ECC: a quad facing away gets a second test on its other half, (fourth, second, third).
    if (!twoSided) {
      const std::uint32_t area = core_.gte().readData(gte::kMac0);
      core_.gte().writeData(gte::kSxy0, screens[3]);
      if (asSigned(area) <= 0) {
        core_.gte().execute(gte::kNclip);
        if (asSigned(core_.gte().readData(gte::kMac0)) >= 0) {
          return Sector::Drawn;
        }
      }
    }
    bin = (depth >> 5) + (colourWord & kBinBiasMask) * 8 + kBinBase;
  }
  if (asSigned(bin - lastBin_) >= 0) {
    return Sector::Drawn;
  }

  constexpr std::array<std::uint32_t, 4> kColourShifts = {23, 16, 9, 2};
  const std::uint32_t prim = frame_.primitive;
  for (std::uint32_t i = 0; i < corners; ++i) {
    std::uint32_t colour = memory_.mem_r32(colours + ((colourWord >> kColourShifts[i]) & 0x1FCu));
    if (i == 0) {
      colour += triangle ? guest_packet::kG3Code : guest_packet::kG4Code;
    }
    memory_.mem_w32(prim + 4 + 8 * i, colour);
    memory_.mem_w32(prim + 8 + 8 * i, screens[i]);
  }
  memory_.mem_w32(prim, triangle ? guest_packet::kG3Tag : guest_packet::kG4Tag);
  return link(bin, triangle ? guest_packet::kG3Bytes : guest_packet::kG4Bytes) ? Sector::Drawn : Sector::BufferFull;
}

// 80028F9C: link the packet just written, unless the buffer is already past its end (then retail
// leaves it written but unlinked and the drawer returns). A bin's first packet may move the mark.
bool FarPass::link(std::uint32_t bin, std::uint32_t bytes) {
  if (asSigned(bufferEnd_ - frame_.primitive) < 0) {
    return false;
  }
  const std::uint32_t prim = frame_.primitive;
  const Linked linked = frame_.linkAndAdvance(bin, PacketSize{bytes});
  if (linked.head == 0 && asSigned(linked.markKey - markBin_) > 0) {
    markBin_ = linked.markKey;
  }
  frame_.v0 = prim >> 16;
  return true;
}

} // namespace

void drawFarSectors(TerrainFrame &frame) {
  FarPass(frame).run();
}

} // namespace spyro::guest_terrain
