#include "spyro1/core/hud/native_pause_menu.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include "core/native_execution.h"
#include "guest_call.h"

#include <cstdint>
#include <string_view>

namespace spyro1::native {
namespace {

using spyro1::guest::kZero3;

using spyro1::guest::kGamestate;
using spyro1::guest::kLevelId;
using spyro1::guest::kLoadStage;
using spyro1::guest::kPad;
using spyro1::guest::kStateSwitch;

// Guest globals, each a lui+immediate the body at 0x8002E12C builds.
constexpr std::uint32_t kIsFlightLevel = 0x80075690u;
constexpr std::uint32_t kNoButtonTicks = 0x8007568Cu;
constexpr std::uint32_t kHudSteadyTicks[] = {
    0x80077FB4u, 0x80077FB8u, 0x80077FBCu, 0x80077FC0u, 0x80077FC4u};
constexpr std::uint32_t kFramesSinceStart = 0x800758B8u;
constexpr std::uint32_t kMenuScreen = 0x800757C8u;
constexpr std::uint32_t kSelectedOption = 0x80075720u;
constexpr std::uint32_t kSpu = 0x800761D4u;
constexpr std::uint32_t kActAvailable = 0x800756D8u;
constexpr std::uint32_t kActEnabled = 0x800757A4u;
constexpr std::uint32_t kShockTicks = 0x80075764u;
constexpr std::uint32_t kSoundVolumeSetting = 0x80075754u;
constexpr std::uint32_t kSoundVolumeScale = 0x80076228u;
constexpr std::uint32_t kSoundVolumeOut = 0x8007622Cu;
constexpr std::uint32_t kMusicVolumeSetting = 0x80075748u;
constexpr std::uint32_t kMusicVolume = 0x80076224u;
// The body derives the attribute block with `addiu $a0,$v0,-0x31C` from the volume word.
constexpr std::uint32_t kSpuCommonAttr = kMusicVolume - 0x31Cu;
constexpr std::uint32_t kMusicVolumeLeft = 0x80075F18u;
constexpr std::uint32_t kMusicVolumeRight = 0x80075F1Au;
constexpr std::uint32_t kAudioMono = 0x80076240u;
constexpr std::uint32_t kCdMixVolume = 0x800776D0u;
constexpr std::uint32_t kCameraMode = 0x80075914u;
constexpr std::uint32_t kOverlaySpacePointer = 0x80076B90u;
constexpr std::uint32_t kCdLoadMode = 0x800113A0u;
constexpr std::uint32_t kCdLoadLength = 0x8007A6E4u;
constexpr std::uint32_t kCdLoadOffset = 0x8007A6E0u;
constexpr std::uint32_t kTitleScreenCopy = 0x8007DDE8u;
constexpr std::uint32_t kCopyBuf = 0x800785D8u;
constexpr std::uint32_t kDiscCopyBuf = 0x800785DCu;
constexpr std::uint32_t kCutsceneIdx = 0x8007566Cu;
constexpr std::uint32_t kUpdateMoby = 0x80075734u;
constexpr std::uint32_t kExitLevelFlag = 0x80075900u;
constexpr std::uint32_t kHeadLookTarget = 0x80078BFCu;
constexpr std::uint32_t kHeadLookField = 0x80078BECu;

// Guest callees, each named by its `jal` site.
constexpr std::uint32_t kHudTick = 0x80054988u;            // `jal` at 0x8002E148
constexpr std::uint32_t kSpecularUpdate = 0x80058CC0u;     // `jal` at 0x8002E18C
constexpr std::uint32_t kPlaySound = 0x80055A78u;          // `jal` at 0x8002E1F4 and every menu row
constexpr std::uint32_t kKillSoundsAndMusic = 0x80056B28u; // `jal` at 0x8002E27C
constexpr std::uint32_t kSpuUpdate = 0x80056ED4u;          // `jal` at 0x8002E2A0
constexpr std::uint32_t kClearImage = 0x8005F8F8u;         // `jal` at 0x8002E2CC
constexpr std::uint32_t kDrawSync = 0x8005F764u;           // `jal` at 0x8002E2D4
constexpr std::uint32_t kSpuSetCommonAttr = 0x8005CC58u;   // `jal` at 0x8002E6C4
constexpr std::uint32_t kCdLoadSync = 0x80016500u;         // `jal` at 0x8002E328
constexpr std::uint32_t kLoadCutscene = 0x80014564u;       // `jal` at 0x8002E338
constexpr std::uint32_t kCdMusicUpdate = 0x8002BBE0u;      // `jal` at 0x8002E340
constexpr std::uint32_t kStartCutscenePlayback = 0x8002D338u; // `jal` at 0x8002E360
constexpr std::uint32_t kKickTitleScreen = 0x8002D170u;       // `jal` at 0x8002E368
constexpr std::uint32_t kOpenInventory = 0x8002C714u;         // `jal` at 0x8002EA34
constexpr std::uint32_t kResumeFromPause = 0x8002C534u;       // `jal` at 0x8002E9F0
constexpr std::uint32_t kExitLevel = 0x8002C618u;             // `jal` at 0x8002EB0C
constexpr std::uint32_t kCdMix = 0x80063FF0u;                 // `jal` at 0x8002E778

// The title's own pad layout, not the console's: Select is 0x100 and Up is 0x1000.
constexpr std::uint32_t kPadTriangle = 0x10u;
constexpr std::uint32_t kPadCross = 0x40u;
constexpr std::uint32_t kPadSelect = 0x100u;
constexpr std::uint32_t kPadUp = 0x1000u;
constexpr std::uint32_t kPadRight = 0x2000u;
constexpr std::uint32_t kPadDown = 0x4000u;
constexpr std::uint32_t kPadLeft = 0x8000u;
constexpr std::uint32_t kPadLeftRight = 0xA000u;
constexpr std::uint32_t kPadLeftRightCross = 0xA040u;
constexpr std::uint32_t kPadStartCross = 0x840u;
constexpr std::uint32_t kPadCrossTriangle = 0x50u;

constexpr std::uint32_t kScreenOptions = 1u;
constexpr std::uint32_t kScreenQuit = 2u;
constexpr std::uint32_t kInputArmFrames = 5;
constexpr std::uint32_t kOptionsRowCount = 6;
constexpr std::uint32_t kMainRowCount = 4;
constexpr std::uint32_t kSettingLimit = 10;
constexpr std::uint32_t kLoadStageTarget = 0xA;
constexpr std::uint32_t kCdLoadTimeout = 600u;
constexpr std::uint32_t kFullScreenWidth = 512;
constexpr std::uint32_t kFullScreenHeight = 480;
constexpr std::uint32_t kMenuCursorSoundOffset = 0x2du;
constexpr std::uint32_t kMenuConfirmSoundOffset = 0x2eu;
constexpr std::uint32_t kSpuCommonCdVolumeMask = 0xC0u;
constexpr std::uint32_t kMusicVolumeShift = 11;
constexpr std::uint32_t kSoundVolumeSettingShift = 14;
constexpr std::uint32_t kSoundVolumeOutShift = 12;
constexpr std::uint32_t kMagicDivideByTen = 0x66666667u;
constexpr std::uint32_t kMagicDivideByVolume = 0x80020009u;
constexpr std::uint32_t kStereoLevel = 0x7Fu;
constexpr std::uint32_t kMonoLevel = 0x3Fu;
constexpr std::uint32_t kSilentLevel = 0u;
constexpr std::uint32_t kPassiveCamera = 2u;
constexpr std::uint32_t kActiveCamera = 0x52u;
constexpr std::uint32_t kExitToTitleState = 7u;

// The body's own 0x30 frame holds the ClearImage RECT and the CDLoadSync stack argument.
constexpr std::uint32_t kFrameBytes = 0x30u;
constexpr std::uint32_t kStackArgumentOffset = 0x10u;
constexpr std::uint32_t kRectOffset = 0x18u;

// Names this override as the native caller of every guest call below.
constexpr std::string_view kOverrideName = "update_pause_menu_input";

// The next guest call reads this call's argument registers, so they are mirrored into the file.
void playMenuSound(Core *c, std::uint32_t soundId) {
  c->r[4] = soundId;
  c->r[5] = 0u;
  c->r[6] = 0x10u;
  c->r[7] = 0u;
  psx::cpu::callGuestNow(*c, kOverrideName, kPlaySound, soundId, 0u, 0x10u, 0u);
}

std::uint32_t menuSoundId(Core *c, std::uint32_t offset) {
  return c->mem_r8(c->mem_r32(kSpu) + offset);
}

// The body's signed divide by ten (`mult` by 0x66666667); its $v0 is always overwritten.
std::int32_t divideByTen(std::int32_t dividend) {
  const std::int64_t product =
      static_cast<std::int64_t>(dividend) * static_cast<std::int32_t>(kMagicDivideByTen);
  const std::uint32_t sign = static_cast<std::uint32_t>(dividend) >> 31;
  const std::uint32_t shifted =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(product >> 32) >> 2);
  return static_cast<std::int32_t>(shifted - sign);
}

// The sound-volume row's same-shape divide by 0x3FFF.
std::uint32_t divideByVolumeScale(std::uint32_t scaled) {
  const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(scaled)) *
                               static_cast<std::int32_t>(kMagicDivideByVolume);
  const std::uint32_t sum = static_cast<std::uint32_t>(product >> 32) + scaled;
  const std::int32_t quotient = static_cast<std::int32_t>(sum) >> kSoundVolumeOutShift;
  return static_cast<std::uint32_t>(quotient) - (scaled >> 31);
}

// 0x8002E12C: void, but every exit leaves the guest's $v0/$v1 pair.
void updatePauseMenuInput(Core *c) {
  if (c->mem_r32(kIsFlightLevel) == 0u) {
    psx::cpu::callGuestNow(*c, kOverrideName, kHudTick, c->r[4], c->r[5], c->r[6], c->r[7]);
  }
  for (const std::uint32_t steady : kHudSteadyTicks) {
    c->mem_w32(steady, 0u);
  }
  c->mem_w32(kNoButtonTicks, c->mem_r32(kNoButtonTicks) + 1u);
  psx::cpu::callGuestNow(*c, kOverrideName, kSpecularUpdate, 3u, c->r[5], c->r[6], c->r[7]);

  // Signed gate over the first fields of a level; its result stays in $v0.
  c->r[2] = static_cast<std::int32_t>(c->mem_r32(kFramesSinceStart)) <
                    static_cast<std::int32_t>(kInputArmFrames)
                ? 1u
                : 0u;
  if (c->r[2] != 0u) {
    return;
  }

  const std::uint32_t pad = c->mem_r32(kPad);
  c->r[4] = pad;
  c->r[2] = pad & kPadSelect;
  if (c->r[2] != 0u) {
    psx::cpu::callGuestNow(*c, kOverrideName, kOpenInventory, 0u, c->r[5], c->r[6], c->r[7]);
    return;
  }

  const std::uint32_t screen = c->mem_r32(kMenuScreen);
  c->r[3] = screen;
  if (screen == kScreenQuit) {
    c->r[2] = pad & kPadLeftRight;
    if (c->r[2] != 0u) {
      // Left or right toggles the two-button quit screen's answer, read back after the sound.
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      const std::uint32_t selected = c->mem_r32(kSelectedOption);
      c->mem_w32(kSelectedOption, 1u - selected);
      c->r[3] = selected;
      c->r[2] = 1u - selected;
      return;
    }
    c->r[2] = pad & kPadCrossTriangle;
    if (c->r[2] == 0u) {
      return;
    }
    c->r[3] = c->mem_r32(kSelectedOption);
    const bool answersNo = c->r[3] == 1u;
    c->r[2] = pad & kPadTriangle;
    if (answersNo || c->r[2] != 0u) {
      playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
      c->mem_w32(kMenuScreen, 0u);
      c->mem_w32(kSelectedOption, 3u);
      c->r[2] = 3u;
      return;
    }

    // Quit confirmed: the title-screen overlay load blocks in a guest poll on the load-stage word.
    psx::cpu::callGuestNow(*c, kOverrideName, kKillSoundsAndMusic, 0u, c->r[5], c->r[6], c->r[7]);
    playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
    psx::cpu::callGuestNow(*c, kOverrideName, kSpuUpdate, c->r[4], c->r[5], c->r[6], c->r[7]);
    const std::uint32_t rect = c->r[29] - kFrameBytes + kRectOffset;
    c->mem_w16(rect, 0u);
    c->mem_w16(rect + 2u, 0u);
    c->mem_w16(rect + 4u, kFullScreenWidth);
    c->mem_w16(rect + 6u, kFullScreenHeight);
    psx::cpu::callGuestNow(*c, kOverrideName, kClearImage, rect, 0u, 0u, 0u);
    psx::cpu::callGuestNow(*c, kOverrideName, kDrawSync, 0u, c->r[5], c->r[6], c->r[7]);
    c->mem_w32(kCopyBuf, kTitleScreenCopy);
    c->mem_w32(kDiscCopyBuf, kTitleScreenCopy);
    c->mem_w32(kLoadStage, 0u);
    c->mem_w32(kCutsceneIdx, 0u);
    c->mem_w32(c->r[29] - kFrameBytes + kStackArgumentOffset, kCdLoadTimeout);
    psx::cpu::callGuestNow(*c,
                           kOverrideName,
                           kCdLoadSync,
                           c->mem_r32(kOverlaySpacePointer),
                           c->mem_r32(kCdLoadMode),
                           c->mem_r32(kCdLoadLength),
                           c->mem_r32(kCdLoadOffset));
    while (static_cast<std::int32_t>(c->mem_r32(kLoadStage)) <
           static_cast<std::int32_t>(kLoadStageTarget)) {
      psx::cpu::callGuestNow(*c, kOverrideName, kLoadCutscene, c->r[4], c->r[5], c->r[6], c->r[7]);
      psx::cpu::callGuestNow(*c, kOverrideName, kCdMusicUpdate, c->r[4], c->r[5], c->r[6], c->r[7]);
    }
    psx::cpu::callGuestNow(
        *c, kOverrideName, kStartCutscenePlayback, c->r[4], c->r[5], c->r[6], c->r[7]);
    psx::cpu::callGuestNow(*c, kOverrideName, kKickTitleScreen, c->r[4], c->r[5], c->r[6], c->r[7]);
    c->mem_w32(kStateSwitch, 1u);
    c->r[2] = 1u;
    return;
  }

  if (screen == kScreenOptions) {
    c->r[2] = pad & kPadDown;
    if (c->r[2] != 0u) {
      playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
      const std::uint32_t selected = c->mem_r32(kSelectedOption);
      c->mem_w32(kNoButtonTicks, 0u);
      const std::uint32_t next = selected + 1u;
      c->r[3] = next;
      c->mem_w32(kSelectedOption, next);
      if (static_cast<std::int32_t>(next) >= static_cast<std::int32_t>(kOptionsRowCount)) {
        c->mem_w32(kSelectedOption, 0u);
      } else if (next == 3u && c->mem_r32(kActAvailable) == 0u) {
        c->mem_w32(kSelectedOption, selected + 2u);
      }
    } else {
      c->r[2] = pad & kPadUp;
      if (c->r[2] != 0u) {
        playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
        const std::uint32_t selected = c->mem_r32(kSelectedOption);
        c->mem_w32(kNoButtonTicks, 0u);
        const std::uint32_t previous = selected - 1u;
        c->r[3] = previous;
        c->mem_w32(kSelectedOption, previous);
        if (static_cast<std::int32_t>(previous) < 0) {
          c->mem_w32(kSelectedOption, kOptionsRowCount - 1u);
        } else if (previous == 3u && c->mem_r32(kActAvailable) == 0u) {
          c->mem_w32(kSelectedOption, selected - 2u);
        }
      }
    }

    // Triangle leaves the options screen, and it shares its body with the last row below.
    c->r[2] = pad & kPadTriangle;
    if (c->r[2] != 0u) {
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      c->mem_w32(kMenuScreen, 0u);
      c->mem_w32(kSelectedOption, 0u);
      return;
    }
    c->r[3] = c->mem_r32(kSelectedOption);
    c->r[2] = c->r[3] < kOptionsRowCount ? 1u : 0u;
    if (c->r[2] == 0u) {
      return;
    }
    switch (c->r[3]) {
    case 0: {
      std::uint32_t volume = c->mem_r32(kSoundVolumeSetting);
      if ((pad & kPadLeft) != 0u) {
        if (static_cast<std::int32_t>(volume) > 0) {
          playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
          volume = c->mem_r32(kSoundVolumeSetting) - 1u;
        }
      } else if ((pad & kPadRight) != 0u) {
        if (static_cast<std::int32_t>(volume) < static_cast<std::int32_t>(kSettingLimit)) {
          playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
          volume = c->mem_r32(kSoundVolumeSetting) + 1u;
        }
      }
      c->mem_w32(kSoundVolumeSetting, volume);
      const std::uint32_t setting = (volume << kSoundVolumeSettingShift) - volume;
      c->r[3] = static_cast<std::uint32_t>(divideByTen(static_cast<std::int32_t>(setting)));
      c->mem_w32(kSoundVolumeScale, c->r[3]);
      c->r[2] = divideByVolumeScale(c->r[3] << kSoundVolumeOutShift);
      c->mem_w32(kSoundVolumeOut, c->r[2]);
      return;
    }
    case 1: {
      std::uint32_t volume = c->mem_r32(kMusicVolumeSetting);
      if ((pad & kPadLeft) != 0u) {
        if (static_cast<std::int32_t>(volume) > 0) {
          playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
          volume = c->mem_r32(kMusicVolumeSetting) - 1u;
        }
      } else if ((pad & kPadRight) != 0u) {
        if (static_cast<std::int32_t>(volume) < static_cast<std::int32_t>(kSettingLimit)) {
          playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
          volume = c->mem_r32(kMusicVolumeSetting) + 1u;
        }
      }
      c->mem_w32(kMusicVolumeSetting, volume);
      c->mem_w32(kSpuCommonAttr, kSpuCommonCdVolumeMask);
      const std::uint32_t shifted = volume << kMusicVolumeShift;
      c->mem_w32(kMusicVolume, shifted);
      c->mem_w16(kMusicVolumeLeft, static_cast<std::uint16_t>(shifted));
      c->mem_w16(kMusicVolumeRight, static_cast<std::uint16_t>(shifted));
      psx::cpu::callGuestNow(*c,
                             kOverrideName,
                             kSpuSetCommonAttr,
                             kSpuCommonAttr,
                             kSpuCommonCdVolumeMask,
                             c->r[6],
                             c->r[7]);
      return;
    }
    case 2: {
      c->r[2] = pad & kPadLeftRightCross;
      if (c->r[2] == 0u) {
        return;
      }
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      const std::uint32_t mono = 1u - c->mem_r32(kAudioMono);
      c->mem_w32(kAudioMono, mono);
      if (mono == 0u) {
        c->mem_w8(kCdMixVolume + 2u, kStereoLevel);
        c->mem_w8(kCdMixVolume, kStereoLevel);
        c->mem_w8(kCdMixVolume + 3u, kSilentLevel);
        c->mem_w8(kCdMixVolume + 1u, kSilentLevel);
      } else {
        c->mem_w8(kCdMixVolume + 3u, kMonoLevel);
        c->mem_w8(kCdMixVolume + 2u, kMonoLevel);
        c->mem_w8(kCdMixVolume + 1u, kMonoLevel);
        c->mem_w8(kCdMixVolume, kMonoLevel);
      }
      psx::cpu::callGuestNow(*c, kOverrideName, kCdMix, kCdMixVolume, 0u, 0x10u, 0u);
      return;
    }
    case 3: {
      if (c->mem_r32(kActAvailable) == 0u) {
        // The row this machine cannot honour, so the cursor is pushed off it instead.
        c->r[2] = 4u;
        c->mem_w32(kSelectedOption, 4u);
        return;
      }
      c->r[2] = pad & kPadLeftRightCross;
      if (c->r[2] == 0u) {
        return;
      }
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      const std::uint32_t enabled = c->mem_r32(kActEnabled);
      c->r[3] = enabled;
      const std::uint32_t toggled = 1u - enabled;
      c->mem_w32(kActEnabled, toggled);
      c->r[2] = toggled;
      if (toggled == 0u) {
        return;
      }
      c->r[2] = static_cast<std::int32_t>(c->mem_r32(kShockTicks)) <
                        static_cast<std::int32_t>(kSettingLimit)
                    ? 1u
                    : 0u;
      if (c->r[2] == 0u) {
        return;
      }
      c->mem_w32(kShockTicks, kSettingLimit);
      c->r[2] = kSettingLimit;
      return;
    }
    case 4: {
      c->r[2] = pad & kPadLeftRightCross;
      if (c->r[2] == 0u) {
        return;
      }
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      c->r[3] = kPassiveCamera;
      c->r[2] = kActiveCamera;
      c->mem_w32(kCameraMode,
                 c->mem_r32(kCameraMode) == kPassiveCamera ? kActiveCamera : kPassiveCamera);
      return;
    }
    case 5: {
      c->r[2] = pad & kPadCross;
      if (c->r[2] == 0u) {
        return;
      }
      playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
      c->mem_w32(kMenuScreen, 0u);
      c->mem_w32(kSelectedOption, 0u);
      return;
    }
    default:
      return;
    }
  }

  c->r[2] = pad & kPadDown;
  if (c->r[2] != 0u) {
    playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
    const std::uint32_t selected = c->mem_r32(kSelectedOption);
    c->mem_w32(kNoButtonTicks, 0u);
    const std::uint32_t next = selected + 1u;
    c->r[3] = next;
    c->mem_w32(kSelectedOption, next);
    if (static_cast<std::int32_t>(next) >= static_cast<std::int32_t>(kMainRowCount)) {
      c->mem_w32(kSelectedOption, 0u);
    }
  } else {
    c->r[2] = pad & kPadUp;
    if (c->r[2] != 0u) {
      playMenuSound(c, menuSoundId(c, kMenuCursorSoundOffset));
      const std::uint32_t selected = c->mem_r32(kSelectedOption);
      c->mem_w32(kNoButtonTicks, 0u);
      const std::uint32_t previous = selected - 1u;
      c->r[3] = previous;
      c->mem_w32(kSelectedOption, previous);
      if (static_cast<std::int32_t>(previous) < 0) {
        c->mem_w32(kSelectedOption, kMainRowCount - 1u);
      }
    }
  }

  c->r[2] = pad & kPadStartCross;
  if (c->r[2] == 0u) {
    return;
  }
  const std::uint32_t selected = c->mem_r32(kSelectedOption);
  c->r[3] = selected;
  if (selected == 1u) {
    playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
    c->mem_w32(kMenuScreen, kScreenOptions);
    c->mem_w32(kSelectedOption, 0u);
    return;
  }
  c->r[2] = static_cast<std::int32_t>(selected) < 2 ? 1u : 0u;
  if (c->r[2] == 0u) {
    if (selected == 2u) {
      c->r[2] = 3u;
      psx::cpu::callGuestNow(*c, kOverrideName, kOpenInventory, 0u, c->r[5], c->r[6], c->r[7]);
      return;
    }
    c->r[2] = 3u;
    if (selected != 3u) {
      return;
    }
    playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
    const std::uint32_t levelId = c->mem_r32(kLevelId);
    const std::uint32_t tenCount =
        static_cast<std::uint32_t>(divideByTen(static_cast<std::int32_t>(levelId)));
    c->r[3] = tenCount;
    // The body compares the level id against (tenths * 5) << 1, which is its modulo-10 test.
    const std::uint32_t fiveTens = (tenCount << 2) + tenCount;
    c->r[2] = fiveTens << 1;
    if (levelId == c->r[2]) {
      // A homeworld has no level to exit to, so the row opens the two-button quit screen instead.
      c->mem_w32(kSelectedOption, 1u);
      c->mem_w32(kMenuScreen, kScreenQuit);
      return;
    }
    psx::cpu::callGuestNow(*c, kOverrideName, kZero3, kHeadLookTarget, 0u, 0x10u, 0u);
    c->r[2] = c->mem_r32(kIsFlightLevel);
    c->mem_w32(kHeadLookField, 0u);
    if (c->r[2] == 0u) {
      psx::cpu::callGuestNow(*c, kOverrideName, kExitLevel, c->r[4], c->r[5], c->r[6], c->r[7]);
      return;
    }
    c->r[2] = kExitToTitleState;
    c->r[3] = c->mem_r32(kUpdateMoby);
    c->mem_w32(kGamestate, kExitToTitleState);
    c->mem_w32(kSelectedOption, 0u);
    c->mem_w32(kNoButtonTicks, 0u);
    c->mem_w32(kExitLevelFlag, 1u);
    psx::cpu::callGuestNow(*c, kOverrideName, c->r[3], c->r[4], c->r[5], c->r[6], c->r[7]);
    return;
  }
  if (selected == 0u) {
    playMenuSound(c, menuSoundId(c, kMenuConfirmSoundOffset));
    psx::cpu::callGuestNow(*c, kOverrideName, kResumeFromPause, 1u, 0u, 0x10u, 0u);
    return;
  }
}

} // namespace

void registerPauseMenuOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, 0x8002E12Cu, "update_pause_menu_input", updatePauseMenuInput);
}

} // namespace spyro1::native
