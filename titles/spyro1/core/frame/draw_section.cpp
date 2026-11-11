#include "spyro1/core/frame/draw_section.h"

#include "core.h"
#include "game.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

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
// TitlescreenDraw (overlay, 0x8007CEE4) up to its fade: 0x8007DCE8 loads g_Fade, and 0x8007DD04
// calls DrawFade with it before the device tail at 0x8007DD0C.
constexpr std::uint32_t kTitlescreenDraw = 0x8007CEE4u;
constexpr std::uint32_t kTitlescreenFade = 0x8007DCE8u;

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
    state_.replayCamera.reset();
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

// g_Camera's projection (+0x00) and view (+0x14) matrices: nine halfwords each, the pad after them
// as the guest left it.
constexpr std::uint32_t kProjectionMatrix = guest::kCamera;
constexpr std::uint32_t kViewMatrix = guest::kCamera + 0x14u;
constexpr std::uint32_t kMatrixHalves = 9u;

void writeMatrix(psx::present::GuestRedraw &redraw,
                 std::uint32_t at,
                 std::span<const std::uint32_t> words) {
  for (std::uint32_t half = 0; half < kMatrixHalves; ++half) {
    redraw.writeHalf(at + 2u * half,
                     static_cast<std::uint16_t>(words[half / 2u] >> (16u * (half % 2u))));
  }
}

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
  // TSM_Demo's card 0x8001E6B8 waits one VSync, so it presents every field and has no in-betweens.
  return state == guest::kGamestatePlaying ||
         (state == guest::kGamestateTitleScreen &&
          core.mem_r32(guest::kTitlescreenMode) != guest::kTitleModeDemo);
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
  if (const std::uint32_t fade = core.mem_r32(guest::kFade); fade != 0) {
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
  const bool reached = redraw.dispatchTo(kTitlescreenDraw, kTitlescreenFade, {});
  lucent::debug("replay",
                "spyro1 section step 0x{:08X} to 0x{:08X}: {}",
                kTitlescreenDraw,
                kTitlescreenFade,
                reached ? "reached" : "refused");
  if (!reached) {
    return false;
  }
  const TitleFade &fade = core.game->frameDriverAs<FrameDriver>().motion().titleFade();
  if (const std::optional<std::uint32_t> level =
          fade.level(core, core.runtime.replayFrame(core)->replaying())) {
    const std::array<std::uint32_t, 4> args{2u, *level, *level, *level};
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
  // The guest built the matrices from whole angles; a blended camera with fractions carries them.
  if (const std::optional<spyro::guest_camera::Angles> &camera = nativeState(core).replayCamera) {
    const spyro::guest_camera::Matrices matrices = terrain::kTerrainImage.camera(core, *camera);
    writeMatrix(redraw, kProjectionMatrix, matrices.projection);
    writeMatrix(redraw, kViewMatrix, matrices.view);
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
