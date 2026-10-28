#include "render/field/guest_sky_drawer.h"

#include "core/native_execution.h"
#include "gte_registers.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_fog.h"
#include "render/terrain/guest_terrain_screen.h"

namespace spyro::guest_sky {
namespace {

namespace gte = psx::gte;
using guest_word::asSigned;
using guest_word::signedHalf;
using guest_word::sra;

// The visibility leaf's byte per sky sector, 0 when the camera's cell cannot see it.
constexpr std::uint32_t kVisibleFlags = 0x1F8003C0u;
// One outcode word per vertex, `sxy << 5 | edges`.
constexpr std::uint32_t kOutcodes = 0x1F800000u;
// What the packet pool must keep free (80029234).
constexpr std::uint32_t kPoolReserve = 0x400u;

// A sector: sphere VXY, then VZ over the radius; the vertex base Y over X; the base Z over the
// colour and vertex counts; the strip stream's offset over its length; then the packed vertices,
// their colours, one word per strip start and the strips' halfwords.
constexpr std::uint32_t kSphereXY = 0x00;
constexpr std::uint32_t kSphereZRadius = 0x04;
constexpr std::uint32_t kBaseXY = 0x08;
constexpr std::uint32_t kBaseZCounts = 0x0C;
constexpr std::uint32_t kStrips = 0x10;
constexpr std::uint32_t kVertices = 0x14;

constexpr std::uint32_t kFlatTag = 0x84000000u;
constexpr std::uint32_t kGouraudTag = 0x86000000u;
// POLY_G3's command byte 0x30 less this is POLY_F3's 0x20.
constexpr std::uint32_t kFlatCommand = 0x10000000u;
constexpr std::uint32_t kChainEnd = 0x80000000u;

// A strip's current triangle: each corner's outcode word and colour ($at/$v0/$v1, $a0/$a1/$a2).
struct Corners {
  std::uint32_t outcode0 = 0;
  std::uint32_t outcode1 = 0;
  std::uint32_t outcode2 = 0;
  std::uint32_t colour0 = 0;
  std::uint32_t colour1 = 0;
  std::uint32_t colour2 = 0;
};

class SkyDrawer {
public:
  SkyDrawer(Core &core,
            const guest_terrain::Facts &terrain,
            const guest_render_globals::Globals &globals)
      : core_(core), gte_(core.gte()), terrain_(terrain), globals_(globals),
        bounds_(presentedBounds(core, terrain)) {}

  void draw(const Facts &sky);

private:
  [[nodiscard]] bool inFront(std::uint32_t sector);
  [[nodiscard]] bool outcodeVertices(std::uint32_t sector);
  [[nodiscard]] bool drawStrips(std::uint32_t sector);
  [[nodiscard]] std::uint32_t outcodeAt(std::uint32_t index) const;
  void emit(const Corners &corners, bool flat);
  void link();

  Core &core_;
  psx::cpu::Gte &gte_;
  const guest_terrain::Facts &terrain_;
  const guest_render_globals::Globals &globals_;
  guest_terrain::ScreenBounds bounds_;

  std::uint32_t first_ = 0;  // $t7
  std::uint32_t last_ = 0;   // $s0
  std::uint32_t cursor_ = 0; // $s1
  std::uint32_t limit_ = 0;  // $s2
  std::uint32_t tag_ = 0;    // $s3
  std::uint32_t fog_ = 0;    // $t9
  // The sector's vertex colours, faded when the level fogs ($t8), and where its strip-start words
  // begin ($s5 once the vertices are outcoded).
  std::uint32_t colours_ = 0;
  std::uint32_t starts_ = 0;
  // The result registers as retail leaves them.
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
};

void SkyDrawer::draw(const Facts &sky) {
  // 800291B0..800291EC
  fog_ = core_.mem_r32(terrain_.detail.fogLevel);
  gte::RotationWords rotation{};
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    rotation[i] = core_.mem_r32(globals_.cameraRotation + 4 * i);
  }
  gte_.loadRotation(rotation);
  gte_.loadTranslation({});

  // 800291F0..80029234
  const std::uint32_t flagsEnd = kVisibleFlags + core_.mem_r32(sky.sectors);
  std::uint32_t sectors = core_.mem_r32(sky.sectors + 4);
  cursor_ = core_.mem_r32(globals_.primitiveCursor) + 4;
  first_ = cursor_;
  last_ = cursor_ - 4;
  tag_ = 0;
  limit_ = core_.mem_r32(globals_.scratchBaseWord) - kPoolReserve;

  // 80029238..8002924C
  for (std::uint32_t flag = kVisibleFlags; flag != flagsEnd; ++flag, sectors += 4) {
    const std::uint32_t sector = core_.mem_r32(sectors);
    if (core_.mem_r8(flag) == 0 || !inFront(sector) || !outcodeVertices(sector)) {
      continue;
    }
    if (!drawStrips(sector)) {
      break;
    }
  }
  link();
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

// 80029250..80029284: the sphere's depth past its radius.
bool SkyDrawer::inFront(std::uint32_t sector) {
  const std::uint32_t zRadius = core_.mem_r32(sector + kSphereZRadius);
  gte_.writeData(gte::kVxy0, core_.mem_r32(sector + kSphereXY));
  gte_.writeData(gte::kVz0, sra(zRadius, 16));
  v0_ = signedHalf(zRadius);
  gte_.execute(gte::kMvmvaRtV0);
  v1_ = core_.mem_r32(sector + kBaseXY);
  return asSigned(gte_.readData(gte::kIr3) - v0_) > 0;
}

// 80029288..80029378: each vertex projected and stored with its edges; false when every vertex
// is outside one edge.
bool SkyDrawer::outcodeVertices(std::uint32_t sector) {
  const std::uint32_t base = core_.mem_r32(sector + kBaseXY);
  const std::uint32_t zCounts = core_.mem_r32(sector + kBaseZCounts);
  const std::uint32_t baseX = signedHalf(base);
  const std::uint32_t baseY = sra(base, 16);
  const std::uint32_t baseZ = sra(zCounts, 16);
  const std::uint32_t vertexCount = zCounts & 0xFFu;
  const auto load = [&](std::uint32_t word) {
    v0_ = (word >> 21) + baseZ;
    v1_ = (baseX - ((word >> 10) & 0x7FFu)) + ((baseY - (word & 0x3FFu)) << 16);
    gte_.writeData(gte::kVxy0, v1_);
    gte_.writeData(gte::kVz0, v0_);
  };

  std::uint32_t word = sector + kVertices;
  load(core_.mem_r32(word));
  std::uint32_t edges = 0xFFFFFFFFu;
  for (std::uint32_t outcode = kOutcodes; outcode != kOutcodes + vertexCount * 4; outcode += 4) {
    gte_.execute(gte::kRtps);
    word += 4;
    const std::uint32_t sxy = gte_.projected(2).word();
    load(core_.mem_r32(word));
    std::uint32_t code = sxy << 5;
    code += bounds_.aboveTop(sxy) ? 1u : 0u;
    code += bounds_.belowBottom(sxy) ? 2u : 0u;
    code += bounds_.atOrLeftOfLeft(sxy) ? 4u : 0u;
    code += bounds_.atOrRightOfRight(sxy) ? 8u : 0u;
    v0_ = (sxy << 16) - (static_cast<std::uint32_t>(bounds_.right()) << 16);
    v1_ = code;
    edges &= code;
    core_.mem_w32(outcode, code);
  }
  colours_ = word;
  starts_ = colours_ + ((zCounts >> 6) & 0x3FCu);
  return (edges & 0xFu) == 0;
}

std::uint32_t SkyDrawer::outcodeAt(std::uint32_t index) const {
  return core_.mem_r32(kOutcodes + index);
}

// 80029380..8002962C: each strip's first triangle from its start word, then one triangle per
// halfword, each keeping two corners of the one before; false when the pool is spent.
bool SkyDrawer::drawStrips(std::uint32_t sector) {
  const std::uint32_t stripWord = core_.mem_r32(sector + kStrips);
  std::uint32_t half = (stripWord >> 16) + starts_;
  const std::uint32_t halfEnd = (stripWord & 0xFFFFu) + half;
  std::uint32_t halfword = core_.mem_r16(half);
  std::uint32_t starts = starts_;
  if (fog_ != 0) {
    v0_ = starts_ + 4;
    v1_ = guest_terrain::fogColours(
        psx::present::EmitMemory(core_), core_, terrain_, colours_, v0_, fog_);
    colours_ = terrain_.foggedColours;
  }
  const auto colourAt = [this](std::uint32_t offset) {
    return core_.mem_r32(colours_ + offset);
  };

  Corners corners;
  std::uint32_t remaining = 0;
  while (true) {
    // 8002941C..800294EC: the strip's next triangle.
    while (remaining != 0) {
      --remaining;
      const std::uint32_t step = halfword << 2;
      half += 2;
      halfword = signedHalf(core_.mem_r16(half));
      if (asSigned(step) < 0) {
        corners.outcode0 = corners.outcode1;
        corners.outcode1 = corners.outcode2;
        corners.colour0 = corners.colour1;
        corners.colour1 = corners.colour2;
      } else {
        corners.outcode0 = corners.outcode2;
        corners.colour0 = corners.colour2;
      }
      corners.outcode2 = outcodeAt(step & 0x3FCu);
      corners.colour2 = colourAt((step >> 8) & 0x1FCu);
      v0_ = corners.outcode1;
      v1_ = corners.outcode2;
      if ((corners.outcode0 & corners.outcode1 & corners.outcode2 & 0xFu) == 0) {
        emit(corners, corners.colour0 == corners.colour1 && corners.colour0 == corners.colour2);
      }
    }
    // 800294F0..80029574: the next strip's start, or the next sector.
    if (half == halfEnd) {
      return true;
    }
    half += 2;
    const std::uint32_t start = core_.mem_r32(starts);
    starts += 4;
    if (asSigned(limit_ - cursor_) <= 0) {
      return false;
    }
    const std::uint32_t pair = halfword;
    halfword = signedHalf(core_.mem_r16(half));
    corners.outcode0 = outcodeAt((pair >> 6) & 0x3FCu);
    corners.outcode1 = outcodeAt((pair << 2) & 0x3FCu);
    corners.outcode2 = outcodeAt((start >> 22) & 0x3FCu);
    v0_ = corners.outcode1;
    v1_ = corners.outcode2;
    remaining = start & 7u;
    if ((corners.outcode0 & corners.outcode1 & corners.outcode2 & 0xFu) != 0) {
      if (remaining != 0) {
        corners.colour1 = colourAt((start >> 8) & 0x1FCu);
        corners.colour2 = colourAt((start >> 1) & 0x1FCu);
      }
      continue;
    }
    // 80029578..8002962C: written before the flat test, so a flat start leaves its third corner
    // one word past its packet.
    core_.mem_w32(cursor_ + 0x08, sra(corners.outcode0, 5));
    core_.mem_w32(cursor_ + 0x10, sra(corners.outcode1, 5));
    core_.mem_w32(cursor_ + 0x18, sra(corners.outcode2, 5));
    // A start is flat when its three colour offsets are one, not its three colours.
    const std::uint32_t offset0 = (start >> 15) & 0x1FCu;
    const std::uint32_t offset1 = (start >> 8) & 0x1FCu;
    const std::uint32_t offset2 = (start >> 1) & 0x1FCu;
    corners.colour0 = colourAt(offset0);
    corners.colour1 = colourAt(offset1);
    corners.colour2 = colourAt(offset2);
    emit(corners, offset0 == offset1 && offset0 == offset2);
  }
}

// 80029480..800294EC / 80029578..8002962C
void SkyDrawer::emit(const Corners &corners, bool flat) {
  core_.mem_w32(last_, tag_ ^ cursor_);
  last_ = cursor_;
  core_.mem_w32(cursor_ + 0x08, sra(corners.outcode0, 5));
  if (flat) {
    core_.mem_w32(cursor_ + 0x04, corners.colour0 - kFlatCommand);
    core_.mem_w32(cursor_ + 0x0C, sra(corners.outcode1, 5));
    core_.mem_w32(cursor_ + 0x10, sra(corners.outcode2, 5));
    tag_ = kFlatTag;
    cursor_ += 0x14;
    return;
  }
  core_.mem_w32(cursor_ + 0x04, corners.colour0);
  core_.mem_w32(cursor_ + 0x0C, corners.colour1);
  core_.mem_w32(cursor_ + 0x10, sra(corners.outcode1, 5));
  core_.mem_w32(cursor_ + 0x14, corners.colour2);
  core_.mem_w32(cursor_ + 0x18, sra(corners.outcode2, 5));
  tag_ = kGouraudTag;
  cursor_ += 0x1C;
}

// 80029630..8002968C: the cursor stored, the chain ended and linked into the bin one past the
// mark, and the mark advanced onto it.
void SkyDrawer::link() {
  core_.mem_w32(globals_.primitiveCursor, cursor_);
  const std::uint32_t mark = core_.mem_r32(globals_.orderingTableMark);
  if (cursor_ != first_) {
    core_.mem_w32(last_, tag_ ^ kChainEnd);
    if (depth_bins::linkIntoBin(psx::present::EmitMemory(core_), mark + 8, first_, last_) != 0) {
      v1_ = first_ >> 16;
    }
  }
  v0_ = mark + 8;
  core_.mem_w32(globals_.orderingTableMark, v0_);
}

} // namespace

void draw(Core &core,
          const Facts &sky,
          const guest_terrain::Facts &terrain,
          const guest_render_globals::Globals &globals) {
  // 80029170..800291AC
  guest_render_globals::spillBorrowedRegisters(core, globals);
  {
    spyro::PreservedReturnAddress returnAddress(core);
    psx::cpu::callGuestNow(
        core,
        sky.overrideName,
        psx::cpu::GuestCallSite::fromJal(sky.visibilityCallSite, terrain.sectorVisibility),
        1u);
  }
  SkyDrawer(core, terrain, globals).draw(sky);
}

} // namespace spyro::guest_sky
