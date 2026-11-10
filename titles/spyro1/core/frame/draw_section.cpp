#include "spyro1/core/frame/draw_section.h"

#include "core.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/terrain_facts.h"

#include <lucent/log.h>

#include <array>
#include <span>

namespace spyro1 {
namespace {

// g_DB[2] and the globals the draw's preamble sets from the buffer it flips to
// (src/gamestates/draw.c).
constexpr std::uint32_t kDb0 = 0x80076EE0u;
constexpr std::uint32_t kDb1 = 0x80076F64u;
constexpr std::uint32_t kDbSize = 0x84u;
constexpr std::uint32_t kDbPolyBuffer = 0x70u;
constexpr std::uint32_t kDbWorldOt = 0x74u;
constexpr std::uint32_t kDbHudOt = 0x78u;
constexpr std::uint32_t kCurDb = 0x80075888u;
constexpr std::uint32_t kWorldOt = 0x80075820u;
constexpr std::uint32_t kHudOt = 0x8007581Cu;
constexpr std::uint32_t kPolyCursor = 0x800757B0u;
constexpr std::uint32_t kUnread = 0x800758B0u;
constexpr std::array<std::uint32_t, 3> kPolyEnds{0x800756FCu, 0x80075710u, 0x80075780u};

constexpr std::uint32_t kIsFlightLevel = 0x80075690u;
constexpr std::uint32_t kDemoMode = 0x80075714u;
constexpr std::uint32_t kFade = 0x80075918u;
constexpr std::uint32_t kScreenBorder = 0x8007570Cu;
constexpr std::uint32_t kBorderSize = 0x800756C0u;

constexpr std::uint32_t kCameraUpdateMatrices = 0x80033C50u;
constexpr std::uint32_t kQueueMobys = 0x800521C0u;
constexpr std::uint32_t kDrawCollectables = 0x80019300u;
constexpr std::uint32_t kDrawDemoText = 0x80018908u;
constexpr std::array<std::uint32_t, 4> kDrawers{0x80019698u, 0x8002B9CCu, 0x80050BD0u, 0x800573C8u};
constexpr std::uint32_t kDrawFade = 0x800190D4u;
constexpr std::uint32_t kDrawBorder = 0x80018F30u;
constexpr std::uint32_t kDrawHud = 0x800189F0u;
// TitlescreenDraw's TSM_Init steps (titlescreen.c): TitlescreenDrawSprite (x, y, sprite, colour),
// Memset, the actor list and emitter, the terrain and the cyclorama.
constexpr std::uint32_t kTitlescreenDrawSprite = 0x8007CD38u;
constexpr std::uint32_t kListActors = 0x8001F158u;
constexpr std::uint32_t kEmitActors = 0x8001F798u;
constexpr std::uint32_t kMemset = 0x80016914u;
constexpr std::uint32_t kSonyImage = 0x8006FCF4u;
constexpr std::uint32_t kSonyMobyQueueBytes = 0x900u;
constexpr std::uint32_t kSonyBufferBytes = 0x1C00u;
constexpr std::uint32_t kCullingDistance = 0x800785D0u; // g_Environment.m_CullingDistance
constexpr std::uint32_t kTitleCulling = 0x1C000u;
constexpr std::uint32_t kDrawTerrain = 0x800258F0u;
constexpr std::uint32_t kDrawCyclorama = 0x8004EBA8u;
constexpr std::uint32_t kViewMatrix = 0x80076DE4u;
constexpr std::uint32_t kProjectionMatrix = 0x80076DD0u;
constexpr std::uint32_t kAllSectors = 0xFFFFFFFFu;

// func_80016784(0x800): links the bins into one chain and returns its head, DrawOTag's argument.
constexpr std::uint32_t kFlattenBins = 0x80016784u;
constexpr std::uint32_t kBinCount = 0x800u;

// What a replay leaves is undone when it returns: the angle fractions the motion set noted for it,
// and the shaded lines' call-site turns, so the real draw counts from where the replays began.
class ReplayScope {
public:
  explicit ReplayScope(NativeState &state) : state_(state), callSites_(state.callSites) {}
  ~ReplayScope() {
    state_.replayAngles.clear();
    state_.callSites = callSites_;
  }
  ReplayScope(const ReplayScope &) = delete;
  ReplayScope &operator=(const ReplayScope &) = delete;
  ReplayScope(ReplayScope &&) = delete;
  ReplayScope &operator=(ReplayScope &&) = delete;

private:
  NativeState &state_;
  shaded_line::CallSites callSites_;
};

bool step(psx::present::GuestRedraw &redraw,
          std::uint32_t entry,
          std::span<const std::uint32_t> args = {}) {
  const bool returned = redraw.dispatch(entry, args);
  lucent::debug(
      "replay", "spyro1 section step 0x{:08X}: {}", entry, returned ? "returned" : "refused");
  return returned;
}

} // namespace

bool DrawSection::replays(Core &core) const {
  const std::uint32_t state = core.mem_r32(guest::kGamestate);
  return state == guest::kGamestatePlaying ||
         (state == guest::kGamestateTitleScreen &&
          core.mem_r32(guest::kTitlescreenMode) == guest::kTitleModeInit);
}

bool DrawSection::drawPlaying(Core &core, psx::present::GuestRedraw &redraw) {
  if (!step(redraw, kQueueMobys)) {
    return false;
  }
  if (core.mem_r32(kIsFlightLevel) == 0 && !step(redraw, kDrawCollectables)) {
    return false;
  }
  if (core.mem_r32(kDemoMode) != 0 && !step(redraw, kDrawDemoText)) {
    return false;
  }
  for (const std::uint32_t drawer : kDrawers) {
    if (!step(redraw, drawer)) {
      return false;
    }
  }
  if (const std::uint32_t fade = core.mem_r32(kFade); fade != 0) {
    const std::array<std::uint32_t, 4> args{2u, fade << 3, fade << 3, fade << 3};
    if (!step(redraw, kDrawFade, args)) {
      return false;
    }
  }
  if ((core.mem_r32(kScreenBorder) != 0 || core.mem_r32(kBorderSize) != 0) &&
      !step(redraw, kDrawBorder)) {
    return false;
  }
  return step(redraw, kDrawHud);
}

bool DrawSection::drawTitle(Core &core, psx::present::GuestRedraw &redraw) {
  const auto sprite =
      [&redraw](std::int32_t x, std::int32_t y, std::int32_t id, std::uint32_t colour) {
        const std::array<std::uint32_t, 4> args{static_cast<std::uint32_t>(x),
                                                static_cast<std::uint32_t>(y),
                                                static_cast<std::uint32_t>(id),
                                                colour};
        return step(redraw, kTitlescreenDrawSprite, args);
      };
  const auto slide = [&core](std::uint32_t table, std::uint32_t index) {
    return static_cast<std::int32_t>(core.mem_r8(table + index));
  };
  const std::uint32_t subState = core.mem_r32(guest::kTitlescreenSubState);
  const std::uint32_t subTick = core.mem_r32(guest::kTitlescreenSubTick);
  bool drawn = true;
  // TitlescreenDraw's TSM_Init sprites: the logo (0), PRESS START (11) blinking, the borders (1,
  // -1).
  if (subState == guest::kTitleLogoIn) {
    const std::uint32_t layout = core.mem_r32(guest::kCutsceneLayout);
    if (static_cast<std::int32_t>(core.mem_r32(layout + guest::kCutsceneLayoutCurrentTick)) >=
        guest::kTitleLogoInTick) {
      drawn = sprite(128, slide(guest::kTitlescreenAnimIn, subTick) - 128, 0, 0);
    }
  } else if (subState == guest::kTitleWaiting) {
    drawn = sprite(128, 0, 0, 0) && sprite(192, 210, 11, (subTick & 0xFu) < 8 ? 2u : 0u);
  } else if (subState == guest::kTitleLogoOut) {
    if (subTick < guest::kTitlescreenAnimSteps) {
      drawn = sprite(128, slide(guest::kTitlescreenAnimOut, subTick) - 128, 0, 0);
    } else {
      const std::int32_t y =
          slide(guest::kTitlescreenAnimIn, subTick - guest::kTitlescreenAnimSteps) - 119;
      drawn = sprite(108, y, 1, 0) && sprite(255, y, -1, 0);
    }
  }
  const std::array<std::uint32_t, 3> mobyQueue{kSonyImage, 0, kSonyMobyQueueBytes};
  const std::array<std::uint32_t, 3> sonyBuffer{kSonyImage, 0, kSonyBufferBytes};
  if (!drawn || !step(redraw, kQueueMobys) || !step(redraw, kListActors) ||
      !step(redraw, kMemset, mobyQueue) || !step(redraw, kEmitActors) ||
      !step(redraw, kMemset, sonyBuffer)) {
    return false;
  }
  redraw.writeWord(kCullingDistance, kTitleCulling);
  const std::array<std::uint32_t, 1> sectors{kAllSectors};
  const std::array<std::uint32_t, 3> cyclorama{kAllSectors, kViewMatrix, kProjectionMatrix};
  if (!step(redraw, kDrawTerrain, sectors) || !step(redraw, kDrawCyclorama, cyclorama)) {
    return false;
  }
  if (const std::uint32_t fade = core.mem_r32(kFade); fade != 0) {
    const std::array<std::uint32_t, 4> args{2u, fade << 4, fade << 4, fade << 4};
    return step(redraw, kDrawFade, args);
  }
  return true;
}

std::optional<std::uint32_t> DrawSection::draw(Core &core,
                                               psx::present::GuestRedraw &redraw) const {
  const ReplayScope scope(nativeState(core));
  const std::uint32_t db = core.mem_r32(kCurDb) == kDb0 ? kDb1 : kDb0;
  const std::uint32_t poly = core.mem_r32(db + kDbPolyBuffer);
  redraw.writeWord(kCurDb, db);
  redraw.writeWord(kUnread, 0);
  redraw.writeWord(kWorldOt, core.mem_r32(db + kDbWorldOt));
  redraw.writeWord(kHudOt, core.mem_r32(db + kDbHudOt));
  redraw.writeWord(kPolyCursor, poly);
  for (const std::uint32_t end : kPolyEnds) {
    redraw.writeWord(end, poly + terrain::kPacketPoolBytes);
  }
  if (!step(redraw, kCameraUpdateMatrices)) {
    return std::nullopt;
  }
  const bool drawn = core.mem_r32(guest::kGamestate) == guest::kGamestatePlaying
                         ? drawPlaying(core, redraw)
                         : drawTitle(core, redraw);
  const std::array<std::uint32_t, 1> bins{kBinCount};
  if (!drawn || !step(redraw, kFlattenBins, bins)) {
    return std::nullopt;
  }
  return core.r[2];
}

bool DrawSection::linked(std::uint32_t head) const {
  // The frame's other chain is PutDrawEnv's packet inside g_DB.
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(head);
  return offset < psx::gpu::mainRamOffsetOf(kDb0) ||
         offset >= psx::gpu::mainRamOffsetOf(kDb1 + kDbSize);
}

} // namespace spyro1
