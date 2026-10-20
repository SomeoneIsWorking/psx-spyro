#include "spyro1/core/hud/native_hud_collectables.h"
#include "spyro1/core/guest_globals.h"

#include "core/native_execution.h"
#include "guest_call.h"

#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kCosTable;
using spyro1::guest::kDeltaTime;
using spyro1::guest::kSinTable;

// g_Hud and its five machines; sub-states and counters sit at negative offsets from each state.
constexpr std::uint32_t kHud = 0x80077FA8u;
constexpr std::uint32_t kGemDisplay = kHud + 0x00u;
constexpr std::uint32_t kDragonDisplay = kHud + 0x01u;
constexpr std::uint32_t kLifeDisplay = kHud + 0x02u;
constexpr std::uint32_t kEggDisplay = kHud + 0x03u;
constexpr std::uint32_t kKeyDisplay = kHud + 0x04u;
constexpr std::uint32_t kGemSubState = kHud + 0x05u;
constexpr std::uint32_t kDragonSubState = kHud + 0x06u;
constexpr std::uint32_t kLifeSubState = kHud + 0x07u;
constexpr std::uint32_t kEggSubState = kHud + 0x08u;
constexpr std::uint32_t kKeySubState = kHud + 0x09u;
constexpr std::uint32_t kGemCounter = kHud + 0x0Cu; // g_Hud.m_GemSteadyTicks
constexpr std::uint32_t kDragonCounter = kHud + 0x10u;
constexpr std::uint32_t kLifeCounter = kHud + 0x14u;
constexpr std::uint32_t kEggCounter = kHud + 0x18u;
constexpr std::uint32_t kKeyCounter = kHud + 0x1Cu;
constexpr std::uint32_t kTally = kHud + 0x20u;
constexpr std::uint32_t kLifeIndex = kHud + 0x28u;
constexpr std::uint32_t kEggCount = kHud + 0x2Cu;
constexpr std::uint32_t kKeyIndex = kHud + 0x30u;
constexpr std::uint32_t kKeyFlag = kHud + 0x34u;
constexpr std::uint32_t kLifeOrbCount = kHud + 0x38u;
constexpr std::uint32_t kSparkTimer = kHud + 0x3Cu;
constexpr std::uint32_t kEggPhase = kHud + 0x40u;
// Life rectangles start twelve entries in: `lui 0x8008` - 0x7BF4 + 0x60.
constexpr std::uint32_t kSpriteRect = kHud + 0x4C4u;
// The guest forms the rectangle's source table as the orb count's own address plus 0x388.
constexpr std::uint32_t kSpriteValues = kLifeOrbCount + 0x388u;

constexpr std::uint32_t kLevelIndex = 0x80075964u;
constexpr std::uint32_t kLevelGemCount = 0x80077420u;
constexpr std::uint32_t kLevelTarget = 0x80075810u;
constexpr std::uint32_t kTreasureTotal = 0x80075830u;
constexpr std::uint32_t kWaitCounter = 0x8007582Cu;
constexpr std::uint32_t kWaitTimer = 0x800758E8u;
constexpr std::uint32_t kRuntimeEntry = 0x800758E4u;
constexpr std::uint32_t kSparkIds = 0x8006E77Cu;
constexpr std::uint32_t kFadeIds = 0x8006E798u;
constexpr std::uint32_t kSpriteValueBytes = 0x8006E7B4u;
constexpr std::uint32_t kSparkTimerByte = 0x80078192u;
constexpr std::uint32_t kSparkShadeByte = 0x80078191u;
constexpr std::uint32_t kStampCounter = 0x8007829Du;
constexpr std::uint32_t kStampShade = 0x8007829Au;
constexpr std::uint32_t kLifeLevelByte = 0x800783A2u;
constexpr std::uint32_t kRuntimeArgs = 0x800783B4u;
constexpr std::uint32_t kTailCounter = 0x800783FDu;
constexpr std::uint32_t kTailCosine = 0x800783F8u;
constexpr std::uint32_t kTailSine = 0x800783F9u;

// Parked sub-state, the twenty-tick settle, the -0x28 met-target start and the mod-nine reciprocal.
constexpr std::uint32_t kSubStateDone = 0x0Du;
constexpr std::uint32_t kSettleTicks = 0x14u;
constexpr std::uint32_t kRestartCounter = 0xFFFFFFD8u;
// 0x80054C68 `lh $v1,-0xbc8($at)`: the level's gem target the tally must match.
constexpr std::uint32_t kGemCollectTarget = 0x8006F438u;
constexpr std::uint32_t kModuloNine = 0x38E38E39u;

// Guest helpers, each a `jal` operand of the body.
constexpr std::uint32_t kPlayId = 0x80054400u;
constexpr std::uint32_t kFadeId = 0x8005445Cu;
constexpr std::uint32_t kSetSubState = 0x800544A8u;
constexpr std::uint32_t kStepCounter = 0x800542E4u;

// The `jal` each nested call stands for; several machines share one through a `j`.
constexpr std::uint32_t kGemPlayJal = 0x80054D1Cu;
constexpr std::uint32_t kDragonPlayJal = 0x80054E98u;
constexpr std::uint32_t kLifePlayJal = 0x8005517Cu;
constexpr std::uint32_t kKeyPlayJal = 0x80055704u;
constexpr std::uint32_t kEggFadeJal = 0x800554A8u;
constexpr std::uint32_t kGemStepJal = 0x80054BE0u;
constexpr std::uint32_t kGemSetJal = 0x80054BF0u;
constexpr std::uint32_t kLifeStepJal = 0x80055084u;
constexpr std::uint32_t kLifeSetJal = 0x80055098u;
// The two `jalr`s through the level's runtime entry return to their own pc + 8.
constexpr std::uint32_t kEggRuntimeReturn = 0x800553F4u;
constexpr std::uint32_t kKeyRuntimeReturn = 0x80055610u;
// The body lowers $sp by 0x38, which its egg machine's argument block (0x10($sp)) sits inside.
constexpr std::uint32_t kFrameBytes = 0x38u;
constexpr std::uint32_t kRuntimeBlockOffset = 0x10u;

// $a3 and $s0..$s2 carried across the body's call sites; the epilogue reloads $s0..$s2.
struct Hud {
  Core *c;
  std::uint32_t a3;
  std::uint32_t s0;
  std::uint32_t s1;
  std::uint32_t s2;
};

// Callees see the body's $s0..$s2, so they are installed before each call.
void installSavedRegisters(const Hud &hud) {
  hud.c->r[16] = hud.s0;
  hud.c->r[17] = hud.s1;
  hud.c->r[18] = hud.s2;
}

// $a3 is caller-saved, so a pass-through call hands on whatever the previous callee left.
void takeLeftoverArgument(Hud &hud) {
  hud.a3 = hud.c->r[7];
}

// g_LevelGemCount[the level now resident], read fresh at each of the guest's own read points.
std::uint32_t levelGemCount(const Hud &hud) {
  return hud.c->mem_r32(kLevelGemCount + 4u * hud.c->mem_r32(kLevelIndex));
}

// Ids are looked up with the sub-state before the step, so id and store differ by one.
std::uint32_t sparkId(Core *c, std::uint32_t sub) {
  return static_cast<std::uint32_t>(c->mem_r16s(kSparkIds + sub * 2u));
}

std::uint32_t fadeId(Core *c, std::uint32_t sub) {
  return static_cast<std::uint32_t>(c->mem_r16s(kFadeIds + sub * 2u));
}

// 0x80054988: signed `mult` by 0x38E38E39 for the mod nine; the shade byte is an arithmetic shift.
void stepSparkTimerAndEggPhase(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t phase = c->mem_r32(kEggPhase) + 1u;
  const std::uint32_t timer = (c->mem_r32(kSparkTimer) - c->mem_r32(kDeltaTime)) & 0xFFu;
  c->mem_w32(kSparkTimer, timer);
  const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(phase)) *
                               static_cast<std::int64_t>(kModuloNine);
  c->lo = static_cast<std::uint32_t>(product);
  c->hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32);
  const std::int32_t high = static_cast<std::int32_t>(c->hi);
  const std::int32_t quotient = (high >> 1) - (static_cast<std::int32_t>(phase) >> 31);
  c->mem_w32(kEggPhase, phase - static_cast<std::uint32_t>(quotient) * 9u);
  const std::uint32_t cosine = c->mem_r16(kCosTable + timer * 2u);
  c->mem_w8(kSparkTimerByte, static_cast<std::uint8_t>(timer));
  c->mem_w8(kSparkShadeByte,
            static_cast<std::uint8_t>(static_cast<std::int32_t>(cosine << 16) >> 25));
}

// 0x80054A30: four chained compares; 1 plays, 2 settles (0x80054AF0 via 0x80054A6C), 3 tallies, 4+ counts.
void stepGemDisplay(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t state = c->mem_r8(kGemDisplay);
  if (state == 0u) {
    if (c->mem_r32(kTally) != levelGemCount(hud)) {
      c->mem_w8(kGemDisplay, 1u);
      c->mem_w8(kGemSubState, 0u);
    }
    return;
  }
  if (state == 1u) {
    const std::uint32_t sub = c->mem_r8(kGemSubState);
    if (sub == kSubStateDone) {
      c->mem_w8(kGemDisplay, 2u);
      c->mem_w8(kGemSubState, 0u);
      c->mem_w32(kGemCounter, 0u);
      if (c->mem_r32(kTally) == levelGemCount(hud)) {
        c->mem_w32(kGemCounter, kRestartCounter);
      }
      return;
    }
    // 0x80054A84's delay slot makes the store at 0x80054AD4 sub+1.
    c->mem_w8(kGemSubState, sub + 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kGemPlayJal, kPlayId),
                               0u,
                               5u,
                               sparkId(c, sub),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 2u) {
    // Settle needs the tally at the level count and the sub-state at zero.
    if (c->mem_r32(kTally) == levelGemCount(hud) && c->mem_r8(kGemSubState) == 0u) {
      const std::uint32_t stepped = c->mem_r32(kGemCounter) + 1u;
      c->mem_w32(kGemCounter, stepped);
      if (stepped != kSettleTicks) {
        return;
      }
      c->mem_w8(kGemDisplay, 3u);
      c->mem_w8(kGemSubState, kSubStateDone);
      return;
    }
    // Catch-up at 0x80054B4C: the arm that resets kGemCounter.
    c->mem_w32(kGemCounter, 0u);
    const std::uint32_t sub = c->mem_r8(kGemSubState);
    // 0x80054B7C: wide rewind needs a difference >= 3 and the sub-state's low five bits clear.
    if (static_cast<std::int32_t>(levelGemCount(hud) - c->mem_r32(kTally)) >= 3 &&
        (sub & 0x1Fu) == 0u) {
      c->mem_w8(kGemSubState, sub - 0x20u);
    } else {
      c->mem_w8(kGemSubState, sub - 0x10u);
    }
    // $s0 stays on the tally; 0x80054BDC zeroes $a3 for the first call only.
    hud.s0 = kTally;
    hud.a3 = 0u;
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kGemStepJal, kStepCounter),
                               0u,
                               4u,
                               c->mem_r32(kTally) + 1u,
                               hud.a3);
    takeLeftoverArgument(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kGemSetJal, kSetSubState),
                               0u,
                               4u,
                               c->mem_r8(kGemSubState),
                               hud.a3);
    takeLeftoverArgument(hud);
    // 0x80054BF8: step 8 from 0xC9, 1 from 0x15, else 1 only while parked on 0xC0 (never stored).
    const std::uint32_t have = c->mem_r32(kTally);
    const std::int32_t difference = static_cast<std::int32_t>(levelGemCount(hud) - have);
    if (difference >= 0xC9) {
      c->mem_w32(kTally, have + 8u);
    } else if (difference >= 0x15 || c->mem_r8(kGemSubState) == 0xC0u) {
      c->mem_w32(kTally, have + 1u);
    }
    // 0x80054C50: only an exact match with sub-state zero parks the display on 4.
    const std::uint32_t target =
        static_cast<std::uint32_t>(c->mem_r16s(kGemCollectTarget + 2u * c->mem_r32(kLevelIndex)));
    if (c->mem_r32(kTally) == target && c->mem_r8(kGemSubState) == 0u) {
      c->mem_w8(kGemDisplay, 4u);
    }
    return;
  }
  if (state == 3u) {
    // 0x80054CAC: short re-arms, caught up parks on zero, else plays the fade entry.
    if (c->mem_r32(kTally) != levelGemCount(hud)) {
      c->mem_w8(kGemDisplay, 1u);
      return;
    }
    const std::uint32_t sub = c->mem_r8(kGemSubState);
    if (sub == 0u) {
      c->mem_w8(kGemDisplay, 0u);
      return;
    }
    // 0x80054CEC's delay slot makes both the stored byte and the lookup index sub-1.
    c->mem_w8(kGemSubState, sub - 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kGemPlayJal, kPlayId),
                               0u,
                               5u,
                               fadeId(c, sub - 1u),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 4u) {
    // 0x80054D2C: signed compare, so a wrapped counter does not park.
    const std::uint32_t counter = c->mem_r32(kGemCounter) + hud.a3;
    c->mem_w32(kGemCounter, counter);
    if (static_cast<std::int32_t>(counter) >= 0xF0) {
      c->mem_w8(kGemDisplay, 3u);
      c->mem_w8(kGemSubState, kSubStateDone);
    }
    return;
  }
  // Display values above four leave the machine untouched.
}

// 0x80054D5C: the dragon machine; its settle parks on the done sub-state, not zero.
void stepDragonDisplay(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t stamp = c->mem_r8(kStampCounter) + 8u;
  c->mem_w8(kStampCounter, static_cast<std::uint8_t>(stamp));
  c->mem_w8(kStampShade,
            static_cast<std::uint8_t>(c->mem_r16(kCosTable + (stamp & 0xFFu) * 2u) >> 8));
  const std::uint32_t state = c->mem_r8(kDragonDisplay);
  if (state == 1u) {
    const std::uint32_t sub = c->mem_r8(kDragonSubState);
    if (sub == kSubStateDone) {
      c->mem_w8(kDragonDisplay, 2u);
      c->mem_w8(kDragonSubState, 0u);
      c->mem_w32(kDragonCounter, kRestartCounter);
      return;
    }
    c->mem_w8(kDragonSubState, sub + 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kDragonPlayJal, kPlayId),
                               5u,
                               3u,
                               sparkId(c, sub),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 2u) {
    const std::uint32_t counter = c->mem_r32(kDragonCounter) + 1u;
    c->mem_w32(kDragonCounter, counter);
    if (counter != kSettleTicks) {
      return;
    }
    c->mem_w8(kDragonDisplay, 3u);
    c->mem_w8(kDragonSubState, kSubStateDone);
    return;
  }
  if (state != 3u) {
    return;
  }
  const std::uint32_t sub = c->mem_r8(kDragonSubState);
  if (sub == 0u) {
    c->mem_w8(kDragonDisplay, 0u);
    return;
  }
  c->mem_w8(kDragonSubState, sub - 1u);
  installSavedRegisters(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kDragonPlayJal, kPlayId),
                             5u,
                             3u,
                             fadeId(c, sub - 1u),
                             hud.a3);
  takeLeftoverArgument(hud);
}

// 0x80054F90: the index advances only once the sub-state reaches 0xC0.
void stepLifeIndex(Hud &hud) {
  Core *c = hud.c;
  hud.a3 = 0u;
  c->mem_w32(kLifeCounter, 0u);
  c->mem_w8(kLifeSubState, c->mem_r8(kLifeSubState) - 0x10u);
  installSavedRegisters(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kLifeStepJal, kStepCounter),
                             8u,
                             2u,
                             c->mem_r32(kLifeIndex) + 1u,
                             hud.a3);
  takeLeftoverArgument(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kLifeSetJal, kSetSubState),
                             8u,
                             2u,
                             c->mem_r8(kLifeSubState),
                             hud.a3);
  takeLeftoverArgument(hud);
  if (c->mem_r8(kLifeSubState) == 0xC0u) {
    c->mem_w32(kLifeIndex, c->mem_r32(kLifeIndex) + 1u);
  }
}

// 0x80054EA0: the life machine; state 2's orb-count arm latches the count to the wait timer.
void stepLifeDisplay(Hud &hud) {
  Core *c = hud.c;
  c->mem_w8(kLifeLevelByte, static_cast<std::uint8_t>(c->mem_r32(kSparkTimer)));
  const std::uint32_t state = c->mem_r8(kLifeDisplay);
  if (state == 0u) {
    if (c->mem_r32(kLifeIndex) != c->mem_r32(kWaitCounter) ||
        c->mem_r32(kLifeOrbCount) != c->mem_r32(kWaitTimer)) {
      c->mem_w8(kLifeDisplay, 1u);
      c->mem_w8(kLifeSubState, 0u);
    }
    return;
  }
  if (state == 1u) {
    const std::uint32_t sub = c->mem_r8(kLifeSubState);
    if (sub == kSubStateDone) {
      c->mem_w8(kLifeDisplay, 2u);
      c->mem_w8(kLifeSubState, 0u);
      c->mem_w32(kLifeCounter, 0u);
      if (c->mem_r32(kLifeIndex) == c->mem_r32(kWaitCounter) &&
          c->mem_r32(kLifeOrbCount) == c->mem_r32(kWaitTimer)) {
        c->mem_w32(kLifeCounter, kRestartCounter);
      }
      return;
    }
    c->mem_w8(kLifeSubState, sub + 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kLifePlayJal, kPlayId),
                               8u,
                               3u,
                               sparkId(c, sub),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 2u) {
    if (c->mem_r32(kLifeIndex) == c->mem_r32(kWaitCounter) && c->mem_r8(kLifeSubState) == 0u) {
      if (c->mem_r32(kLifeOrbCount) != c->mem_r32(kWaitTimer)) {
        c->mem_w32(kLifeCounter, 0u);
        c->mem_w32(kLifeOrbCount, c->mem_r32(kWaitTimer));
        return;
      }
      const std::uint32_t counter = c->mem_r32(kLifeCounter) + 1u;
      c->mem_w32(kLifeCounter, counter);
      if (counter != kSettleTicks) {
        return;
      }
      c->mem_w8(kLifeDisplay, 3u);
      c->mem_w8(kLifeSubState, kSubStateDone);
      return;
    }
    stepLifeIndex(hud);
    return;
  }
  if (state != 3u) {
    return;
  }
  if (c->mem_r32(kLifeIndex) != c->mem_r32(kWaitCounter) ||
      c->mem_r32(kLifeOrbCount) != c->mem_r32(kWaitTimer)) {
    c->mem_w8(kLifeDisplay, 1u);
    c->mem_w32(kLifeOrbCount, c->mem_r32(kWaitTimer));
    return;
  }
  const std::uint32_t sub = c->mem_r8(kLifeSubState);
  if (sub == 0u) {
    c->mem_w8(kLifeDisplay, 0u);
    return;
  }
  c->mem_w8(kLifeSubState, sub - 1u);
  installSavedRegisters(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kLifePlayJal, kPlayId),
                             8u,
                             3u,
                             fadeId(c, sub - 1u),
                             hud.a3);
  takeLeftoverArgument(hud);
}

// 0x80055184: the word pointer (0x8005519C) never advances, so every orb reads the same x/y words.
void rebuildLifeOrbRects(Hud &hud) {
  Core *c = hud.c;
  const std::int32_t count = static_cast<std::int32_t>(c->mem_r32(kLifeOrbCount));
  if (count <= 0) {
    return;
  }
  for (std::int32_t index = 0; index < count; ++index) {
    const std::uint32_t step = static_cast<std::uint32_t>(index);
    const std::uint32_t rect = kSpriteRect + step * 8u;
    const std::uint32_t x =
        c->mem_r8(kSpriteValueBytes + step * 2u) + c->mem_r32(kSpriteValues) - 0x1Du;
    const std::uint32_t y =
        c->mem_r8(kSpriteValueBytes + step * 2u + 1u) + c->mem_r32(kSpriteValues + 4u) - 0x1Cu;
    c->mem_w16(rect, static_cast<std::uint16_t>(x));
    c->mem_w16(rect + 2u, static_cast<std::uint16_t>(y));
    c->mem_w16(rect + 4u, 8u);
    c->mem_w16(rect + 6u, 8u);
  }
}

// 0x800553AC: call through the level's pointer; the 0x1000 word is stored in the `jalr` delay slot.
void callRuntimeWithCount(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t block = c->r[29] + kRuntimeBlockOffset;
  c->mem_w32(block, c->mem_r32(kEggCount) * 324u - 2500u);
  c->mem_w32(block + 4u, 0x3F0u);
  c->mem_w32(block + 8u, 0x1000u);
  installSavedRegisters(hud);
  c->r[31] = kEggRuntimeReturn;
  psx::cpu::callGuestNow(
      *c, "update_hud_collectables", c->mem_r32(kRuntimeEntry), 0x10u, 0x4Du, block, 0u);
  takeLeftoverArgument(hud);
}

// 0x8005533C: the counter word compares signed (`slti $v0,$v0,9` at 0x800553A8).
void stepEggCount(Hud &hud) {
  Core *c = hud.c;
  if (c->mem_r32(kEggCount) == c->mem_r32(kLevelTarget)) {
    if (c->mem_r32(kKeyFlag) == 0u) {
      const std::uint32_t counter = c->mem_r32(kEggCounter);
      if (counter != kSettleTicks) {
        c->mem_w32(kEggCounter, counter + 1u);
        return;
      }
      c->mem_w8(kEggDisplay, 3u);
      c->mem_w8(kEggSubState, kSubStateDone);
      c->mem_w32(kEggCounter, counter + 1u);
      return;
    }
    c->mem_w32(kEggCounter, 1u);
    return;
  }
  const std::int32_t counter = static_cast<std::int32_t>(c->mem_r32(kEggCounter));
  if (counter == 1) {
    callRuntimeWithCount(hud);
  } else if (counter < 9) {
    c->mem_w32(kEggCounter, static_cast<std::uint32_t>(counter) + 1u);
    return;
  } else {
    c->mem_w32(kEggCount, c->mem_r32(kEggCount) + 1u);
    c->mem_w32(kEggCounter, 0u);
  }
  c->mem_w32(kEggCounter, c->mem_r32(kEggCounter) + 1u);
}

// 0x80055228: the egg machine; its playing paths negate the id.
void stepEggDisplay(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t state = c->mem_r8(kEggDisplay);
  if (state == 0u) {
    if (c->mem_r32(kEggCount) != c->mem_r32(kLevelTarget) || c->mem_r32(kKeyFlag) != 0u) {
      c->mem_w8(kEggDisplay, 1u);
      c->mem_w8(kEggSubState, 0u);
    }
    return;
  }
  if (state == 1u) {
    const std::uint32_t sub = c->mem_r8(kEggSubState);
    if (sub == kSubStateDone) {
      c->mem_w8(kEggDisplay, 2u);
      c->mem_w32(kEggCounter, 0u);
      if (c->mem_r32(kEggCount) == c->mem_r32(kLevelTarget) && c->mem_r32(kKeyFlag) == 0u) {
        c->mem_w32(kEggCounter, kRestartCounter);
      }
      return;
    }
    c->mem_w8(kEggSubState, sub + 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kEggFadeJal, kFadeId),
                               0u,
                               0xCu,
                               0u - sparkId(c, sub),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 2u) {
    stepEggCount(hud);
    return;
  }
  if (state != 3u) {
    return;
  }
  if (c->mem_r32(kEggCount) != c->mem_r32(kLevelTarget) || c->mem_r32(kKeyFlag) != 0u) {
    c->mem_w8(kEggDisplay, 1u);
    return;
  }
  const std::uint32_t sub = c->mem_r8(kEggSubState);
  if (sub == 0u) {
    c->mem_w8(kEggDisplay, 0u);
    return;
  }
  c->mem_w8(kEggSubState, sub - 1u);
  installSavedRegisters(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kEggFadeJal, kFadeId),
                             0u,
                             0xCu,
                             0u - fadeId(c, sub - 1u),
                             hud.a3);
  takeLeftoverArgument(hud);
}

// 0x800555DC: six calls, $s0 from `lui 0x600` stepping by `lui 0x100`; $s0..$s2 restored.
void stepRuntimeTargets(Hud &hud) {
  Core *c = hud.c;
  hud.s1 = 0u;
  hud.s2 = 0x8080u;
  hud.s0 = 0x00600000u;
  do {
    // `addiu $s1,$s1,1` is the `jalr`'s delay slot, so the callee already sees the counter stepped.
    hud.s1 += 1u;
    installSavedRegisters(hud);
    c->r[31] = kKeyRuntimeReturn;
    psx::cpu::callGuestNow(*c,
                           "update_hud_collectables",
                           c->mem_r32(kRuntimeEntry),
                           1u,
                           0xCu,
                           kRuntimeArgs,
                           hud.s0 + hud.s2);
    takeLeftoverArgument(hud);
    hud.s0 += 0x01000000u;
  } while (static_cast<std::int32_t>(hud.s1) < 6);
}

// 0x800555AC: the counter is stepped once on the way out, so the latched arm lands on one.
void stepKeyCounter(Hud &hud, std::uint32_t total) {
  Core *c = hud.c;
  if (c->mem_r32(kKeyIndex) == total) {
    if (c->mem_r32(kKeyCounter) != kSettleTicks) {
      c->mem_w32(kKeyCounter, c->mem_r32(kKeyCounter) + 1u);
      return;
    }
    c->mem_w8(kKeyDisplay, 3u);
    c->mem_w8(kKeySubState, kSubStateDone);
    c->mem_w32(kKeyCounter, c->mem_r32(kKeyCounter) + 1u);
    return;
  }
  const std::int32_t counter = static_cast<std::int32_t>(c->mem_r32(kKeyCounter));
  if (counter == 0) {
    stepRuntimeTargets(hud);
  } else if (counter < 12) {
    c->mem_w32(kKeyCounter, static_cast<std::uint32_t>(counter) + 1u);
    return;
  } else {
    c->mem_w8(kTailCounter, 0x40u);
    c->mem_w32(kKeyCounter, 0u);
    c->mem_w32(kKeyIndex, total);
  }
  c->mem_w32(kKeyCounter, c->mem_r32(kKeyCounter) + 1u);
}

// 0x800554B0: the key machine; the total latches into the index unless it is 1.
void stepKeyDisplay(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t total = c->mem_r32(kTreasureTotal);
  if (total != 1u) {
    c->mem_w32(kKeyIndex, total);
  }
  const std::uint32_t state = c->mem_r8(kKeyDisplay);
  if (state == 0u) {
    if (c->mem_r32(kKeyIndex) != total) {
      c->mem_w8(kKeyDisplay, 1u);
      c->mem_w8(kKeySubState, 0u);
    }
    return;
  }
  if (state == 1u) {
    const std::uint32_t sub = c->mem_r8(kKeySubState);
    if (sub == kSubStateDone) {
      c->mem_w8(kKeyDisplay, 2u);
      c->mem_w32(kKeyCounter, 0u);
      if (c->mem_r32(kKeyIndex) == total) {
        c->mem_w32(kKeyCounter, kRestartCounter);
      }
      return;
    }
    c->mem_w8(kKeySubState, sub + 1u);
    installSavedRegisters(hud);
    psx::cpu::callGuestNow(*c,
                               "update_hud_collectables",
                               psx::cpu::GuestCallSite::fromJal(kKeyPlayJal, kPlayId),
                               0xBu,
                               1u,
                               0u - sparkId(c, sub),
                               hud.a3);
    takeLeftoverArgument(hud);
    return;
  }
  if (state == 2u) {
    stepKeyCounter(hud, total);
    return;
  }
  if (state != 3u) {
    return;
  }
  if (c->mem_r32(kKeyIndex) != total) {
    c->mem_w8(kKeyDisplay, 1u);
    c->mem_w8(kKeySubState, 0u);
    return;
  }
  const std::uint32_t sub = c->mem_r8(kKeySubState);
  if (sub == 0u) {
    c->mem_w8(kKeyDisplay, 0u);
    return;
  }
  c->mem_w8(kKeySubState, sub - 1u);
  installSavedRegisters(hud);
  psx::cpu::callGuestNow(*c,
                             "update_hud_collectables",
                             psx::cpu::GuestCallSite::fromJal(kKeyPlayJal, kPlayId),
                             0xBu,
                             1u,
                             0u - fadeId(c, sub - 1u),
                             hud.a3);
  takeLeftoverArgument(hud);
}

// 0x8005570C: the shared tail that sets the caller-visible v0/v1; the guard counter caps at 99.
void advanceFrameStamp(Hud &hud) {
  Core *c = hud.c;
  const std::uint32_t stamp = c->mem_r8(kTailCounter) + 4u;
  c->mem_w8(kTailCounter, static_cast<std::uint8_t>(stamp));
  const std::uint32_t index = (stamp & 0xFFu) * 2u;
  c->mem_w8(kTailCosine, static_cast<std::uint8_t>(c->mem_r16(kCosTable + index) >> 7));
  c->mem_w8(kTailSine, static_cast<std::uint8_t>(c->mem_r16(kSinTable + index) >> 7));
  const std::uint32_t timer = c->mem_r32(kWaitTimer);
  c->r[3] = timer;
  if (static_cast<std::int32_t>(timer) < 0x14) {
    c->r[2] = 1u;
    return;
  }
  const std::uint32_t counter = c->mem_r32(kWaitCounter) + 1u;
  c->mem_w32(kWaitCounter, counter);
  if (static_cast<std::int32_t>(counter) >= 0x64) {
    c->mem_w32(kWaitCounter, 0x63u);
  }
  c->mem_w32(kWaitTimer, timer - 0x14u);
  c->r[2] = timer - 0x14u;
}

// 0x80054988: the HUD tick; saved registers restored as the retail epilogue does.
void updateHudCollectables(Core *c) {
  const spyro::PreservedReturnAddress returnAddress(*c);
  const spyro::GuestFrameScope frame(*c, c->r[29] - kFrameBytes);
  const std::uint32_t s0 = c->r[16];
  const std::uint32_t s1 = c->r[17];
  const std::uint32_t s2 = c->r[18];
  Hud hud{c, c->mem_r32(kDeltaTime), s0, s1, s2};
  stepSparkTimerAndEggPhase(hud);
  stepGemDisplay(hud);
  stepDragonDisplay(hud);
  stepLifeDisplay(hud);
  rebuildLifeOrbRects(hud);
  stepEggDisplay(hud);
  stepKeyDisplay(hud);
  advanceFrameStamp(hud);
  c->r[16] = s0;
  c->r[17] = s1;
  c->r[18] = s2;
}

} // namespace

void registerHudCollectableOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, 0x80054988u, "update_hud_collectables", updateHudCollectables);
}

} // namespace spyro1::native
