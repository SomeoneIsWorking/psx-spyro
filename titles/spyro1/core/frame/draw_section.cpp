#include "spyro1/core/frame/draw_section.h"

#include "core.h"
#include "game.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "spyro1/core/frame/dragon_shrink.h"
#include "spyro1/core/frame/draw_motion.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/frame/screen_fade.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"
#include "spyro1/render/portals.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"

#include <lucent/log.h>

#include <array>
#include <span>

namespace spyro1 {
namespace {

// g_DB[2] and the globals the draw's preamble sets from the buffer it flips to
// (src/gamestates/draw.c).
constexpr std::uint32_t kDbSize = 0x84u;
constexpr std::uint32_t kDbWorldOt = 0x74u;
constexpr std::uint32_t kDbHudOt = 0x78u;
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
// func_8001E9C8, GS_Cutscene: 0x8001EA88 loads g_Fade for its DrawFade(2, g_Fade * 16, ...).
constexpr std::uint32_t kCutsceneDraw = 0x8001E9C8u;
constexpr std::uint32_t kCutsceneFade = 0x8001EA88u;
// func_8001A40C, the menus: D_800758B8 counts its draws; the first stores the scene greyed into
// VRAM, every later one opens with PutDrawEnv, which the real frame applied, and ends with
// DrawOTag(func_80016784(0)) at 0x8001C598.
constexpr std::uint32_t kMenuDraw = 0x8001A40Cu;
constexpr std::uint32_t kPutDrawEnv = 0x8005FDD8u;
constexpr std::uint32_t kMenuFlatten = 0x8001C598u;
// Flight5, GS_FlightResults through D_8007567C: the same code at the same address in all five
// flight overlays; like the menus it counts its draws in D_800758B8, opens a later one with
// PutDrawEnv and ends with DrawOTag(func_80016784(0)) at 0x8007CEC8. A row's reveal plays its sound
// from the draw (D_8007569C below the row), the real draw's work; every site passes a3 = 0 and sets
// v0 after.
constexpr std::uint32_t kFlightResultsDraw = 0x8007B68Cu;
constexpr std::uint32_t kFlightResultsFlatten = 0x8007CEC8u;
// The credits draw is chosen by `slti g_CreditsStage, 99` at 0x8001EF54.
// func_credits_8007BFD0, GS_Credits while g_CreditsStage is past its first frame (-1, a MoveImage)
// and below 99: its letters, then DrawFade(2, g_Fade << 4) called at 0x8007C1F0 when g_Fade is set
// (beqz 0x8007C1E0), then HUD mobys, terrain and sky up to its DrawSync at 0x8007C260. The letter
// loop falls through into the fade's test, so the fade is stopped at DrawFade's entry and resumed
// at the call's return.
constexpr std::uint32_t kCreditsStage = 0x80075704u;
constexpr std::uint32_t kCreditsFirstStage = 0xFFFFFFFFu;
constexpr std::int32_t kCreditsReturningHome = 99;
constexpr std::uint32_t kCreditsDraw = 0x8007BFD0u;
constexpr std::uint32_t kCreditsFadeReturn = 0x8007C1F8u;
constexpr std::uint32_t kCreditsTail = 0x8007C260u;
// func_8001EB80, the credits from stage 99: RETURNING HOME... spun by g_CreditsTimer, its HUD
// mobys, then DrawSync at 0x8001ECA4.
constexpr std::uint32_t kReturningHomeDraw = 0x8001EB80u;
constexpr std::uint32_t kReturningHomeTail = 0x8001ECA4u;

bool creditsFlyby(Core &core) {
  return static_cast<std::int32_t>(core.mem_r32(kCreditsStage)) < kCreditsReturningHome;
}

// The draws D_800758B8 counts, which link their packets straight into the overlay pair and flatten
// no bins.
bool countedDraw(std::uint32_t state) {
  return guest::menuDraw(state) || state == guest::kGamestateFlightResults;
}

// LevelTransition_Draw, GS_LevelTransition and GS_ArrivalCamera: tally, Spyro and the sky join at
// its DrawSync, 0x8001A334.
constexpr std::uint32_t kLevelTransitionDraw = 0x8001A050u;
constexpr std::uint32_t kLevelTransitionTail = 0x8001A334u;
// MenuTransition_Draw, GS_ExitLevel, switches on g_nMenuDrawFrames: pass 0 opens with PutDrawEnv,
// LoadImage of the backdrop CLUT and DrawSync, pass 1 with the fog store and PutDrawEnv; both then
// draw Spyro and reach LinkOTPrimitives(0x800) at 0x8001C96C, before its DrawOTag.
constexpr std::uint32_t kExitLevelDraw = 0x8001C694u;
constexpr std::uint32_t kExitLevelTail = 0x8001C96Cu;
constexpr std::uint32_t kExitLevelBackdropPass = 0u;
constexpr std::uint32_t kLoadImage = 0x8005FA28u;
constexpr std::uint32_t kDrawSync = 0x8005F764u;

// func_8001CFDC, GS_Dragon: every m_State's drawers, fade and border join at its DrawSync,
// 0x8001D644.
constexpr std::uint32_t kDragonDraw = 0x8001CFDCu;
constexpr std::uint32_t kDragonTail = 0x8001D644u;
constexpr std::uint32_t kDragonShrinkState = 5u;
// func_8001D718, GS_Fairy: its drawers, menu text and border end at its DrawSync, 0x8001E168.
constexpr std::uint32_t kFairyDraw = 0x8001D718u;
constexpr std::uint32_t kFairyTail = 0x8001E168u;
// func_8001E24C, GS_Balloonist: its dialogue, mobys and border end at its DrawSync, 0x8001E5E4.
constexpr std::uint32_t kBalloonistDraw = 0x8001E24Cu;
constexpr std::uint32_t kBalloonistTail = 0x8001E5E4u;

// func_80016784(0x800): links the bins into one chain and returns its head, DrawOTag's argument.
constexpr std::uint32_t kFlattenBins = 0x80016784u;
constexpr std::uint32_t kBinCount = 0x800u;

// What a replay leaves is undone when it returns: the angle fractions the motion set noted for it
// and its blended camera.
class ReplayScope {
public:
  explicit ReplayScope(NativeState &state) : state_(state) {}
  ~ReplayScope() {
    state_.replayAngles.clear();
    state_.replayCamera.reset();
  }
  ReplayScope(const ReplayScope &) = delete;
  ReplayScope &operator=(const ReplayScope &) = delete;
  ReplayScope(ReplayScope &&) = delete;
  ReplayScope &operator=(ReplayScope &&) = delete;

private:
  NativeState &state_;
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

const DrawMotion &motion(Core &core) {
  return core.game->frameDriverAs<FrameDriver>().motion();
}

// The fade level this replay draws, `t` of the way between the ticks' levels.
std::optional<std::uint32_t> fadeLevel(Core &core) {
  return motion(core).screenFade().level(core, core.runtime.replayFrame(core)->replaying());
}

} // namespace

bool DrawSection::replays(Core &core) const {
  const std::uint32_t state = core.mem_r32(guest::kGamestate);
  // TSM_Demo's card 0x8001E6B8 and RespawnOrGameOver_Draw 0x8001CA38 wait one VSync, so they
  // present every field and have no in-betweens.
  return state == guest::kGamestatePlaying || state == guest::kGamestateDragon ||
         state == guest::kGamestateFairy || state == guest::kGamestateBalloonist ||
         state == guest::kGamestateCutscene || state == guest::kGamestateLevelTransition ||
         state == guest::kGamestateArrivalCamera || state == guest::kGamestateExitLevel ||
         (state == guest::kGamestateCredits && core.mem_r32(kCreditsStage) != kCreditsFirstStage) ||
         (countedDraw(state) && core.mem_r32(guest::kMenuDraws) != 0) ||
         (state == guest::kGamestateTitleScreen &&
          core.mem_r32(guest::kTitlescreenMode) != guest::kTitleModeDemo);
}

std::uint32_t DrawSection::stage(Core &core) {
  const std::uint32_t state = core.mem_r32(guest::kGamestate);
  return state == guest::kGamestateDragon ? state << 16u | core.mem_r32(guest::kDragonCutsceneState)
                                          : state << 16u;
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
  if (const std::optional<std::uint32_t> level = fadeLevel(core)) {
    const std::array<std::uint32_t, 4> args{2u, *level, *level, *level};
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

bool DrawSection::drawTo(psx::present::GuestRedraw &redraw,
                         std::uint32_t entry,
                         std::uint32_t end,
                         std::span<const psx::present::GuestRedraw::Skip> skips) {
  const bool reached = redraw.dispatchTo(entry, end, {}, skips);
  lucent::debug("replay",
                "spyro1 section step 0x{:08X} to 0x{:08X}: {}",
                entry,
                end,
                reached ? "reached" : "refused");
  return reached;
}

bool DrawSection::drawToFade(Core &core,
                             psx::present::GuestRedraw &redraw,
                             std::uint32_t entry,
                             std::uint32_t fade) {
  if (!drawTo(redraw, entry, fade)) {
    return false;
  }
  if (const std::optional<std::uint32_t> level = fadeLevel(core)) {
    const std::array<std::uint32_t, 4> args{2u, *level, *level, *level};
    return step(redraw, kDrawFade, args);
  }
  return true;
}

bool DrawSection::drawCredits(Core &core, psx::present::GuestRedraw &redraw) {
  if (core.mem_r32(guest::kFade) == 0) {
    return drawTo(redraw, kCreditsDraw, kCreditsTail);
  }
  bool faded = true;
  const std::array<psx::present::GuestRedraw::Skip, 1> fade{
      {{.at = kDrawFade, .resume = kCreditsFadeReturn, .host = [&] {
          if (const std::optional<std::uint32_t> level = fadeLevel(core)) {
            const std::array<std::uint32_t, 4> args{2u, *level, *level, *level};
            faded = step(redraw, kDrawFade, args);
          }
        }}}};
  return drawTo(redraw, kCreditsDraw, kCreditsTail, fade) && faded;
}

bool DrawSection::drawDragon(Core &core, psx::present::GuestRedraw &redraw) {
  if (core.mem_r32(guest::kDragonCutsceneState) != kDragonShrinkState) {
    return drawTo(redraw, kDragonDraw, kDragonTail);
  }
  const DragonShrink &shrink = motion(core).dragonShrink();
  const std::optional<float> t = core.runtime.replayFrame(core)->replaying();
  const std::array<psx::present::GuestRedraw::Skip, 1> scale{
      {{.at = DragonShrink::kScaleBlock, .resume = DragonShrink::kScaleBlockEnd, .host = [&] {
          shrink.scale(core, redraw, t);
        }}}};
  return drawTo(redraw, kDragonDraw, kDragonTail, scale);
}

bool DrawSection::drawExitLevel(Core &core, psx::present::GuestRedraw &redraw) {
  // The real frame put the backdrop CLUT and draw environment; a fraction of a CLUT shift has no
  // upload, so the backdrop's darkening shows the current tick's.
  if (core.mem_r32(guest::kMenuDraws) == kExitLevelBackdropPass) {
    const std::array<psx::present::GuestRedraw::Skip, 3> applied{
        {{.at = kPutDrawEnv}, {.at = kLoadImage}, {.at = kDrawSync}}};
    return drawTo(redraw, kExitLevelDraw, kExitLevelTail, applied);
  }
  const std::optional<std::uint32_t> progress =
      motion(core).exitFade().progress(core.runtime.replayFrame(core)->replaying());
  const std::array<psx::present::GuestRedraw::Skip, 1> fogged{
      {{.at = kPutDrawEnv, .host = [&] {
          if (progress) {
            redraw.writeWord(portals::kFadeStep, *progress);
          }
        }}}};
  return drawTo(redraw, kExitLevelDraw, kExitLevelTail, fogged);
}

bool DrawSection::drawState(Core &core, psx::present::GuestRedraw &redraw) {
  if (guest::menuDraw(core.mem_r32(guest::kGamestate))) {
    const std::array<psx::present::GuestRedraw::Skip, 1> applied{{{.at = kPutDrawEnv}}};
    return drawTo(redraw, kMenuDraw, kMenuFlatten, applied);
  }
  switch (core.mem_r32(guest::kGamestate)) {
  case guest::kGamestatePlaying:
    return drawPlaying(core, redraw);
  case guest::kGamestateDragon:
    return drawDragon(core, redraw);
  case guest::kGamestateFlightResults: {
    const std::array<std::uint32_t, 1> sound{guest::kPlaySound};
    redraw.applied(sound);
    const std::array<psx::present::GuestRedraw::Skip, 1> applied{{{.at = kPutDrawEnv}}};
    return drawTo(redraw, kFlightResultsDraw, kFlightResultsFlatten, applied);
  }
  case guest::kGamestateLevelTransition:
  case guest::kGamestateArrivalCamera:
    return drawTo(redraw, kLevelTransitionDraw, kLevelTransitionTail);
  case guest::kGamestateExitLevel:
    return drawExitLevel(core, redraw);
  case guest::kGamestateFairy:
    return drawTo(redraw, kFairyDraw, kFairyTail);
  case guest::kGamestateBalloonist:
    return drawTo(redraw, kBalloonistDraw, kBalloonistTail);
  case guest::kGamestateCutscene:
    return drawToFade(core, redraw, kCutsceneDraw, kCutsceneFade);
  case guest::kGamestateCredits:
    return creditsFlyby(core) ? drawCredits(core, redraw)
                              : drawTo(redraw, kReturningHomeDraw, kReturningHomeTail);
  default:
    return drawToFade(core, redraw, kTitlescreenDraw, kTitlescreenFade);
  }
}

std::optional<std::uint32_t> DrawSection::draw(Core &core,
                                               psx::present::GuestRedraw &redraw) const {
  const ReplayScope scope(nativeState(core));
  nativeState(core).drawnRecords.beginDraw(actors::DrawnRecords::Draw::Replay);
  const std::uint32_t db = core.mem_r32(guest::kCurDb) == guest::kDb0 ? guest::kDb1 : guest::kDb0;
  const std::uint32_t poly = core.mem_r32(db + guest::kDbPolyBuffer);
  redraw.writeWord(guest::kCurDb, db);
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
  const bool drawn = drawState(core, redraw);
  const std::array<std::uint32_t, 1> bins{countedDraw(core.mem_r32(guest::kGamestate)) ? 0u
                                                                                       : kBinCount};
  if (!drawn || !step(redraw, kFlattenBins, bins)) {
    return std::nullopt;
  }
  return core.r[2];
}

bool DrawSection::linked(std::uint32_t head) const {
  // The frame's other chain is PutDrawEnv's packet inside g_DB.
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(head);
  return offset < psx::gpu::mainRamOffsetOf(guest::kDb0) ||
         offset >= psx::gpu::mainRamOffsetOf(guest::kDb1 + kDbSize);
}

} // namespace spyro1
