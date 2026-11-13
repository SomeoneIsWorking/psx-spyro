#pragma once

#include "core/boot_prefix_runtime.h"
#include "core/debug/debug_menu.h"
#include "core/debug/moby_teleport.h"
#include "spyro3/core/boot_facts.h"
#include "spyro3/core/draw_section.h"
#include "spyro3/core/warp.h"
#include "spyro3/render/render_facts.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

namespace spyro3 {

// Runtime owner for SCUS_944.67. Its executable runs directly; the shared owners it uses are the
// field owner, the archive transfer and the stock libraries.
class Runtime final : public spyro::BootPrefixRuntime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  const spyro::FollowCameraFacts &followCamera() const override;
  const spyro::TitleLogoFacts &logoFacts() const override;
  // The 2D font (render/text_font.h).
  std::span<const psx::present::OutlineFont> outlineFonts() const override;
  const psx::game::TitleDebugMenu &debugMenu() const override {
    return debugMenu_;
  }

protected:
  const spyro::DrawSection &drawSection() const override {
    return section_;
  }
  const spyro::DrawMotionFacts &drawMotion() const override;
  std::unique_ptr<spyro::Warp> createWarp() const override;
  std::unique_ptr<spyro::TitleState> createTitleState() const override;

private:
  DrawSection section_;
  psx::hle::StockVSync vsync_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
  spyro::RecordMobyTeleport mobys_{{.gameState = kGameState,
                                    .mobysWord = kMobyFacts.firstRecordWord,
                                    .spyro = kDrawMotion.spyro}};
  // The cheat matcher 0x80017A40 calls the effect with the matched row; row 1 rolls the credits
  // (0x80054CD8, state 10).
  spyro::DebugMenu debugMenu_{&Warp::worlds,
                              &mobys_,
                              kBootPrefixFacts.cut,
                              {.title = kTitleScreenGameState},
                              {.effect = 0x80017B7Cu, .credits = 1u}};
};

} // namespace spyro3
