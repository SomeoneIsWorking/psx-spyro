// The widened culls and HUD anchoring read the presentation plan the Game latched, so a new session
// of the same runtime starts unwidened; and each title binds libgte's leaves at its own addresses.
#include "booted_title.h"
#include "game.h"
#include "guest_widescreen_projection.h"
#include "platform_hle.h"
#include "testutil.h"
#include "ui_anchor.h"

#include <lucent/log.h>

#include <cstdint>
#include <exception>

namespace {

// The plan the Record presenter latches for a 512x240 display at 16:9.
GuestProjectionPlan widePlan() {
  return guest_projection_plan({
      .path = RenderPath::Record,
      .requested = PresentationAspect::Wide16x9,
      .nativePresentation = {512, 240},
      .nativeProjection = {.extent = {512, 240}, .drawWidth = 512},
      .sink = {1920, 1080},
      .vramWidth = 1024,
  });
}

void test_the_hud_frame_follows_the_sessions_plan() {
  spyro::test::BootedTitle first(spyro::SpyroTitle::Spyro2);
  const spyro::ui_anchor::Frame unwidened = spyro::ui_anchor::widescreenFrame(first.core(), 512);
  CHECK_EQ(unwidened.authored, 512);
  CHECK_EQ(unwidened.drawn, 512);
  first.game().guestDisplay.latch(widePlan());
  const spyro::ui_anchor::Frame widened = spyro::ui_anchor::widescreenFrame(first.core(), 512);
  CHECK_EQ(widened.authored, 512);
  CHECK_EQ(widened.drawn, 684);
}

void test_a_new_session_starts_unwidened() {
  spyro::test::BootedTitle title(spyro::SpyroTitle::Spyro3);
  title.game().guestDisplay.latch(widePlan());
  title.restart();
  const spyro::ui_anchor::Frame frame = spyro::ui_anchor::widescreenFrame(title.core(), 512);
  CHECK_EQ(frame.drawn, frame.authored);
}

void checkLibgteLeaves(spyro::SpyroTitle title,
                       std::uint32_t setGeomOffset,
                       std::uint32_t setGeomScreen) {
  spyro::test::BootedTitle booted(title);
  const PlatformHle &table = booted.game().platform_hle;
  CHECK(table.lookup(setGeomOffset) != nullptr);
  if (setGeomScreen != 0) {
    CHECK(table.lookup(setGeomScreen) != nullptr);
  }
}

void test_each_title_binds_its_own_libgte_leaves() {
  checkLibgteLeaves(spyro::SpyroTitle::Spyro1, 0x80062618u, 0x80062638u);
  checkLibgteLeaves(spyro::SpyroTitle::Spyro2, 0x80057AF8u, 0x80057AE8u);
  // Spyro 3's one CR26 write takes an argument computed at its call site; it stays guest code.
  checkLibgteLeaves(spyro::SpyroTitle::Spyro3, 0x8005D35Cu, 0);
}

} // namespace

int main() {
  try {
    RUN(the_hud_frame_follows_the_sessions_plan);
    RUN(a_new_session_starts_unwidened);
    RUN(each_title_binds_its_own_libgte_leaves);
    return pt_summary();
  } catch (const std::exception &error) {
    lucent::error("test_guest_widescreen", "FAIL: {}", error.what());
    return 1;
  }
}
