#include "spyro1/core/moby/native_moby_transform.h"

#include "spyro1/core/guest_globals.h"
#include "core/native_execution.h"
#include "guest_call.h"
#include "game.h"

#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kCosTable;
using spyro1::guest::kFrameLag;
using spyro1::guest::kMobyCollisionChain;
using spyro1::guest::kModels;
using spyro1::guest::kSinTable;

// Moby fields: +0x36 model class, +0x38 per-class lists, +0x3C/+0x3E the animation entry bytes.
constexpr std::uint32_t kBucketLink = 0x04u;
constexpr std::uint32_t kPositionRow = 0x08u;
constexpr std::uint32_t kRotationWords = 0x0Cu;
constexpr std::uint32_t kOrientationMatrix = 0x20u;
constexpr std::uint32_t kBucketIndex = 0x34u;
constexpr std::uint32_t kModelClass = 0x36u;
constexpr std::uint32_t kModelListArray = 0x38u;
constexpr std::uint32_t kAnimationLowByte = 0x3Cu;
constexpr std::uint32_t kAnimationHighByte = 0x3Eu;
constexpr std::uint32_t kEntryFlag = 0x41u;
constexpr std::uint32_t kPackedAngles = 0x44u;

// g_Models entry: +0x14 is the row table; bits 19..23 of the row selector pick one of eight rows.
constexpr std::uint32_t kModelRowTable = 0x14u;
constexpr std::uint32_t kRowSelectorMask = 0x1Cu;
constexpr std::uint32_t kRowSelectorShift = 19u;
// Per-class list: flag byte at the head; entries are 8 bytes (`sll 3`).
constexpr std::uint32_t kListEntryFlag = 0x0Cu;
constexpr std::uint32_t kEntryRow = 0x24u;

// g_MobyCollisionChain: a head per bucket, the bucket index kept in the moby at +0x34.
constexpr std::uint32_t kNoBucket = 0xFFFFu;
// Bucket index: two rotation words shifted down 13; a non-negative chain head adds a half turn.
constexpr std::uint32_t kRotationShift = 13u;
constexpr std::uint32_t kBucketHalfTurn = 0x400u;

// Angles at +0x44 as halfword table offsets: bits 15..23, then byte 1 and byte 0 doubled.
constexpr std::uint32_t kFirstAngleShift = 15u;
constexpr std::uint32_t kFirstAngleMask = 0x1FEu;
constexpr std::uint32_t kSecondAngleMask = 0xFF00u;
constexpr std::uint32_t kThirdAngleMask = 0xFFu;

// MVMVA sf=12, mx=0, v=0, cv=3, lm=0, run six times over an identity 4.12 matrix.
constexpr std::uint32_t kMultiplyVector = 0x4A486012u;
constexpr std::uint32_t kUnitDiagonal = 0x1000u;
// VXY0/VZ0 inputs; DR9..DR11 are IR1..IR3.
constexpr std::uint32_t kGteVectorXY0 = 0u;
constexpr std::uint32_t kGteVectorZ0 = 1u;
constexpr std::uint32_t kGteIrX = 9u;
constexpr std::uint32_t kGteIrY = 10u;
constexpr std::uint32_t kGteIrZ = 11u;
// Each angle replaces one 16-bit half of a matrix word and keeps the other.
constexpr std::uint32_t kHighHalf = 0xFFFF0000u;
constexpr std::uint32_t kLowHalf = 0xFFFFu;

// Bank written directly: the guest MTC2 keeps all 32 bits of DR1; writeData narrows it.
void writeVectorZ0(Core *c, std::uint32_t value) {
  c->gte().registers().REG[kGteVectorZ0] = value;
}

// 0x800526A8: +0x34 is stored in the `beqz` delay slot; the bucket lookup uses the full index.
void rebuildMobyTransform(Core *c) {
  const std::uint32_t moby = c->r[4];
  const std::uint32_t model = c->mem_r32(kModels + (c->mem_r16(moby + kModelClass) << 2));
  const std::uint32_t list =
      c->mem_r32(model + (c->mem_r8(moby + kAnimationLowByte) << 2) + kModelListArray);
  const std::uint32_t entryFlag = c->mem_r8(list + kListEntryFlag);
  const std::uint32_t rowSelector =
      c->mem_r32(list + (c->mem_r8(moby + kAnimationHighByte) << 3) + kEntryRow);
  c->mem_w8(moby + kEntryFlag, entryFlag);
  c->mem_w32(
      moby + kPositionRow,
      c->mem_r32(model + kModelRowTable + ((rowSelector >> kRowSelectorShift) & kRowSelectorMask)));
  const std::uint32_t linked = c->mem_r32(model + kModelRowTable);

  c->mem_w16(moby + kBucketIndex, kNoBucket);
  if (linked != 0u) {
    const std::uint32_t lowRotation = c->mem_r32(moby + kRotationWords) >> kRotationShift;
    const std::uint32_t highRotation = c->mem_r32(moby + kRotationWords + 4u) >> kRotationShift;
    const std::uint32_t halfTurn =
        static_cast<std::int32_t>(linked) < 0 ? 0u : static_cast<std::uint32_t>(kBucketHalfTurn);
    const std::uint32_t index = lowRotation + (highRotation << 5u) + halfTurn;
    c->mem_w16(moby + kBucketIndex, static_cast<std::uint16_t>(index));
    const std::uint32_t head = c->mem_r32(kMobyCollisionChain) + (index << 2);
    const std::uint32_t previous = c->mem_r32(head);
    c->mem_w32(head, moby);
    c->mem_w32(moby + kBucketLink, previous);
  }

  const std::uint32_t angles = c->mem_r32(moby + kPackedAngles);
  c->r[2] = kSinTable;
  c->r[3] = kCosTable;
  std::uint32_t cr0 = kUnitDiagonal;
  std::uint32_t cr1 = 0u;
  std::uint32_t cr2 = kUnitDiagonal;
  std::uint32_t cr3 = 0u;
  std::uint32_t cr4 = kUnitDiagonal;
  c->gte().writeControl(0u, cr0);
  c->gte().writeControl(1u, cr1);
  c->gte().writeControl(2u, cr2);
  c->gte().writeControl(3u, cr3);
  c->gte().writeControl(4u, cr4);
  c->gte().writeControl(5u, 0u);
  c->gte().writeControl(6u, 0u);
  c->gte().writeControl(7u, 0u);

  const std::uint32_t firstIndex = (angles >> kFirstAngleShift) & kFirstAngleMask;
  if (firstIndex != 0u) {
    const std::uint32_t x = c->mem_r16(kCosTable + firstIndex);
    const std::uint32_t y = c->mem_r16(kSinTable + firstIndex);
    c->gte().writeData(kGteVectorXY0, x);
    writeVectorZ0(c, y);
    c->gte().execute(kMultiplyVector);
    // The halves go back swapped: Y negated, X as is.
    const std::uint32_t mirrored = (0u - y) & kLowHalf;
    const std::uint32_t firstX = c->gte().readData(kGteIrX);
    const std::uint32_t firstY = c->gte().readData(kGteIrY);
    const std::uint32_t firstZ = c->gte().readData(kGteIrZ);
    c->gte().writeData(kGteVectorXY0, mirrored);
    writeVectorZ0(c, x);
    c->gte().execute(kMultiplyVector);
    cr0 = (cr0 & kHighHalf) + (firstX & kLowHalf);
    cr3 = (cr3 & kHighHalf) + (firstZ & kLowHalf);
    const std::uint32_t firstYHigh = firstY << 16u;
    const std::uint32_t secondX = c->gte().readData(kGteIrX);
    const std::uint32_t secondY = c->gte().readData(kGteIrY);
    const std::uint32_t secondZ = c->gte().readData(kGteIrZ);
    cr1 = (secondX & kLowHalf) + firstYHigh;
    cr2 = (cr2 & kLowHalf) + (secondY << 16u);
    cr4 = secondZ & kLowHalf;
    c->gte().writeControl(0u, cr0);
    c->gte().writeControl(1u, cr1);
    c->gte().writeControl(2u, cr2);
    c->gte().writeControl(3u, cr3);
    c->gte().writeControl(4u, cr4);
  }

  const std::uint32_t secondIndex = ((angles & kSecondAngleMask) >> 8u) << 1u;
  if (secondIndex != 0u) {
    const std::uint32_t x = c->mem_r16(kCosTable + secondIndex);
    const std::uint32_t y = c->mem_r16(kSinTable + secondIndex);
    c->gte().writeData(kGteVectorXY0, x << 16u);
    writeVectorZ0(c, y);
    c->gte().execute(kMultiplyVector);
    const std::uint32_t mirrored = 0u - (y << 16u);
    const std::uint32_t firstX = c->gte().readData(kGteIrX);
    const std::uint32_t firstY = c->gte().readData(kGteIrY);
    const std::uint32_t firstZ = c->gte().readData(kGteIrZ);
    c->gte().writeData(kGteVectorXY0, mirrored);
    writeVectorZ0(c, x);
    c->gte().execute(kMultiplyVector);
    cr0 = (cr0 & kLowHalf) + (firstX << 16u);
    cr3 = (cr3 & kLowHalf) + (firstZ << 16u);
    const std::uint32_t firstYLow = firstY & kLowHalf;
    const std::uint32_t secondX = c->gte().readData(kGteIrX);
    const std::uint32_t secondY = c->gte().readData(kGteIrY);
    const std::uint32_t secondZ = c->gte().readData(kGteIrZ);
    cr1 = (cr1 & kHighHalf) + (secondX & kLowHalf);
    cr2 = (secondY << 16u) + firstYLow;
    cr4 = secondZ & kLowHalf;
    c->gte().writeControl(0u, cr0);
    c->gte().writeControl(1u, cr1);
    c->gte().writeControl(2u, cr2);
    c->gte().writeControl(3u, cr3);
    c->gte().writeControl(4u, cr4);
  }

  const std::uint32_t thirdIndex = (angles & kThirdAngleMask) << 1u;
  if (thirdIndex != 0u) {
    const std::uint32_t x = c->mem_r16(kCosTable + thirdIndex);
    const std::uint32_t y = c->mem_r16(kSinTable + thirdIndex);
    c->gte().writeData(kGteVectorXY0, (y << 16u) + x);
    writeVectorZ0(c, 0u);
    c->gte().execute(kMultiplyVector);
    const std::uint32_t packed = ((0u - y) & kLowHalf) + (x << 16u);
    const std::uint32_t firstX = c->gte().readData(kGteIrX);
    const std::uint32_t firstY = c->gte().readData(kGteIrY);
    const std::uint32_t firstZ = c->gte().readData(kGteIrZ);
    c->gte().writeData(kGteVectorXY0, packed);
    writeVectorZ0(c, 0u);
    c->gte().execute(kMultiplyVector);
    const std::uint32_t firstYHigh = firstY << 16u;
    const std::uint32_t secondX = c->gte().readData(kGteIrX);
    const std::uint32_t secondY = c->gte().readData(kGteIrY);
    const std::uint32_t secondZ = c->gte().readData(kGteIrZ);
    cr0 = (secondX << 16u) + (firstX & kLowHalf);
    cr1 = (cr1 & kLowHalf) + firstYHigh;
    cr2 = (cr2 & kHighHalf) + (secondY & kLowHalf);
    cr3 = (secondZ << 16u) + (firstZ & kLowHalf);
  }

  c->mem_w32(moby + kOrientationMatrix + 0u, cr0);
  c->mem_w32(moby + kOrientationMatrix + 4u, cr1);
  c->mem_w32(moby + kOrientationMatrix + 8u, cr2);
  c->mem_w32(moby + kOrientationMatrix + 12u, cr3);
  c->mem_w32(moby + kOrientationMatrix + 16u, cr4);
}


// 0x80037F90: v1 is filled from a2 in a delay slot; an unknown width returns 1 with v1 untouched.

void tickMobyTimer(Core *c) {
  const std::uint32_t timerAddr = c->r[4];
  const std::uint32_t timerWidth = c->r[5];
  if (timerWidth != 4u && timerWidth != 2u && timerWidth != 1u) {
    c->r[2] = 1;
    return;
  }
  const std::int32_t delta = static_cast<std::int32_t>(c->mem_r32(kFrameLag));
  if (timerWidth == 4u) {
    const std::uint32_t timer = c->mem_r32(timerAddr);
    c->r[3] = timer;
    if (delta < static_cast<std::int32_t>(timer)) {
      c->mem_w32(timerAddr, timer - static_cast<std::uint32_t>(delta));
      c->r[2] = 0;
    } else if (timer != 0u) {
      c->mem_w32(timerAddr, 0);
      c->r[2] = 2;
    } else {
      c->r[2] = 1;
    }
    return;
  }
  if (timerWidth == 2u) {
    const std::int32_t timer = c->mem_r16s(timerAddr);
    c->r[3] = static_cast<std::uint32_t>(timer);
    if (delta < timer) {
      c->mem_w16(timerAddr, static_cast<std::uint16_t>(timer - delta));
      c->r[2] = 0;
    } else if (timer != 0) {
      c->mem_w16(timerAddr, 0);
      c->r[2] = 2;
    } else {
      c->r[2] = 1;
    }
    return;
  }
  const std::int32_t timer = c->mem_r8(timerAddr);
  c->r[3] = static_cast<std::uint32_t>(timer);
  if (delta < timer) {
    c->mem_w8(timerAddr, static_cast<std::uint8_t>(timer - delta));
    c->r[2] = 0;
  } else if (timer != 0) {
    c->mem_w8(timerAddr, 0);
    c->r[2] = 2;
  } else {
    c->r[2] = 1;
  }
}

constexpr std::uint32_t kRenderRadius = 0x50u;
constexpr std::uint32_t kDepthOffset = 0x47u;
constexpr std::uint32_t kNextAnimationFrame = 0x3fu;
constexpr std::uint32_t kUpdateDistance = 0x52u;
constexpr std::uint32_t kPerFrameAnimationProgress = 0x41u;
constexpr std::uint32_t kPodIndex = 0x43u;
constexpr std::uint32_t kSectorIndex = 0x4au;
constexpr std::uint32_t kDropMobyIndex = 0x53u;
constexpr std::uint32_t kRenderer = 0x4bu;
constexpr std::uint32_t kSoundChannel = 0x54u;
constexpr std::uint32_t kShadowDistance = 0x38u;
constexpr std::uint32_t kSpecularMetalColour = 0x4cu;
constexpr std::uint32_t kZeroedWordFields[] = {0x18u, 0x1cu};
constexpr std::uint32_t kZeroedByteFields[] = {
    0x3cu, 0x3du, 0x3eu, 0x40u, 0x44u, 0x45u, 0x46u, 0x48u, 0x49u, 0x57u};

// 0x8003A720: void, but leaves v0 = 0x7F and v1 = 0xFF from its last stores.
void resetMobyDefaults(Core *c) {
  const std::uint32_t moby = c->r[4];

  c->mem_w8(moby + kRenderRadius, 16);
  c->mem_w8(moby + kDepthOffset, 4);
  c->mem_w8(moby + kNextAnimationFrame, 1);
  c->mem_w8(moby + kUpdateDistance, 0xff);
  c->mem_w8(moby + kPerFrameAnimationProgress, 32);
  c->mem_w8(moby + kPodIndex, 0xff);
  c->mem_w8(moby + kSectorIndex, 0xff);
  c->mem_w8(moby + kDropMobyIndex, 0xff);
  c->mem_w8(moby + kRenderer, 32);
  c->mem_w8(moby + kSoundChannel, 0x7f);
  c->mem_w16(moby + kShadowDistance, 0);
  c->mem_w32(moby + kSpecularMetalColour, 0);
  for (const std::uint32_t word : kZeroedWordFields) {
    c->mem_w32(moby + word, 0);
  }
  for (const std::uint32_t field : kZeroedByteFields) {
    c->mem_w8(moby + field, 0);
  }

  c->r[2] = 0x7fu;
  c->r[3] = 0xffu;
}

constexpr std::uint32_t kMobyPosition = 0x0cu;
constexpr std::uint32_t kMobyPositionZ = 0x14u;
constexpr std::uint32_t kGroundProbeHeight = 0x5dcu;
constexpr std::uint32_t kGroundProbeRadius = 0x1000u;
constexpr std::uint32_t kGroundProbeFrame = 0x18u;
// Snapshots its caller's s0..s7, gp, sp, fp and ra into 0x3C017DD8.
constexpr std::uint32_t kFindGroundHeightBelow = 0x8004D5ECu;       // `jal` at 0x80038360
constexpr std::uint32_t kFindGroundHeightBelowReturn = 0x80038368u; // the `jal`'s own pc + 8

// 0x80038340: z bump stored in the `jal` delay slot; runs in retail's 0x18 frame, s0 and ra set.
void mobyGroundHeight(Core *c) {
  const std::uint32_t moby = c->r[4];
  const std::uint32_t callerSp = c->r[29];
  const std::uint32_t callerS0 = c->r[16];
  const std::uint32_t callerRa = c->r[31];
  c->r[29] = callerSp - kGroundProbeFrame;
  c->r[16] = moby; // `move $s0, $a0`
  c->r[31] = kFindGroundHeightBelowReturn;
  c->mem_w32(moby + kMobyPositionZ, c->mem_r32(moby + kMobyPositionZ) + kGroundProbeHeight);
  psx::cpu::callGuestNow(
      *c, "moby_ground_height", kFindGroundHeightBelow, moby + kMobyPosition, kGroundProbeRadius);
  const std::uint32_t restored = c->mem_r32(moby + kMobyPositionZ) - kGroundProbeHeight;
  c->mem_w32(moby + kMobyPositionZ, restored);
  c->r[16] = callerS0;
  c->r[31] = callerRa;
  c->r[29] = callerSp;
  c->r[3] = restored;
}

// 0x80037E98: retail body is `jr $ra ; nop`.
void mobyInterpolationCheck(Core *) {}

} // namespace

void registerMobyTransformOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x80037E98u, "moby_interpolation_check", mobyInterpolationCheck);
  psx::cpu::installNativeOverride(core, 0x80037F90u, "tick_moby_timer", tickMobyTimer);
  psx::cpu::installNativeOverride(core, 0x80038340u, "moby_ground_height", mobyGroundHeight);
  psx::cpu::installNativeOverride(core, 0x8003A720u, "reset_moby_defaults", resetMobyDefaults);
  psx::cpu::installNativeOverride(
      core, 0x800526A8u, "rebuild_moby_transform", rebuildMobyTransform);
}

} // namespace spyro1::native
