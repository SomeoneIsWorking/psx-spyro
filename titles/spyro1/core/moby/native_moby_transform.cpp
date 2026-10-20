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

// The Moby's own fields. +0x36 is the model class the model table is indexed by, +0x38 that model's
// per-class list array, and +0x3C/+0x3E the low and high byte of the animation word that pick one
// 8-byte entry in one of those lists.
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

// One g_Models entry: its +0x14 word is the row table this body reads a row of as the moby's
// position, and the chain head that decides whether the moby joins a bucket at all. The row read is
// the same +0x14 word plus bits 19..23 of the entry's row selector, which select one of eight rows.
constexpr std::uint32_t kModelRowTable = 0x14u;
constexpr std::uint32_t kRowSelectorMask = 0x1Cu;
constexpr std::uint32_t kRowSelectorShift = 19u;
// One 8-byte entry of a per-class list: a flag byte at the head of the list itself, and the row
// selector in the entry the two animation bytes pick (`sll 3`, the entry stride).
constexpr std::uint32_t kListEntryFlag = 0x0Cu;
constexpr std::uint32_t kEntryRow = 0x24u;

// g_MobyCollisionChain: a head per bucket, the bucket index kept in the moby at +0x34.
constexpr std::uint32_t kNoBucket = 0xFFFFu;
// The bucket index is two packed words of rotation, each shifted down 13 bits, and a non-negative
// chain head adds a half turn.
constexpr std::uint32_t kRotationShift = 13u;
constexpr std::uint32_t kBucketHalfTurn = 0x400u;

// The three angles packed into the word at +0x44, as halfword offsets into the sin and cos tables:
// the first from bits 15..23 (already an even halfword count), the second from byte 1 and the third
// from byte 0, each of those doubled.
constexpr std::uint32_t kFirstAngleShift = 15u;
constexpr std::uint32_t kFirstAngleMask = 0x1FEu;
constexpr std::uint32_t kSecondAngleMask = 0xFF00u;
constexpr std::uint32_t kThirdAngleMask = 0xFFu;

// The one GTE command the body runs, six times: MVMVA with sf=12, matrix 0, vector 0, the
// translation pair cv=3 and lm=0. The matrix starts as 1.0 on the diagonal in the GTE's 16.12
// fixed point, the identity each angle's rotation is composed onto.
constexpr std::uint32_t kMultiplyVector = 0x4A486012u;
constexpr std::uint32_t kUnitDiagonal = 0x1000u;
// The two data registers the MVMVA reads VX0 from (the decomp's C2_VXY0 and C2_VZ0), and the three
// the body reads each result back from: DR9..DR11 are IR1..IR3.
constexpr std::uint32_t kGteVectorXY0 = 0u;
constexpr std::uint32_t kGteVectorZ0 = 1u;
constexpr std::uint32_t kGteIrX = 9u;
constexpr std::uint32_t kGteIrY = 10u;
constexpr std::uint32_t kGteIrZ = 11u;
// Each angle contributes a NEW 16.16 half to a matrix word and keeps the half the angle before it
// left there, so the five words are assembled from the three angles' partial products.
constexpr std::uint32_t kHighHalf = 0xFFFF0000u;
constexpr std::uint32_t kLowHalf = 0xFFFFu;

// The body's `mtc2 ..., C2_VZ0` writes land in DR1, and this is the ONE register where the
// executor's MTC2 and Gte::writeData disagree on what is left behind: the backend stores the
// source register's full 32 bits, while GTE_WriteDR narrows DR1 to a sign-extended halfword. The
// GTE reads DR1 as a halfword either way, so the matrix is identical and only the register the
// differential compares differs: a 0x0000F000 from the guest against a 0xFFFFF000 from the port.
// Writing the bank directly is what reproduces it.
void writeVectorZ0(Core *c, std::uint32_t value) {
  c->gte().registers().REG[kGteVectorZ0] = value;
}

// 0x800526A8 — re-derive one moby's world transform. The model table its class selects names the
// row this moby takes (one +0x14 word of that model, read twice: plain, and again through the
// rotation bits of its animation entry's row selector) and the chain head that decides whether it
// joins a collision-chain bucket at all — an index built from two packed rotation words, under
// which it files itself at the head. The three angles packed at +0x44 then rebuild the 3x3
// orientation matrix at +0x20, each running the GTE twice: once over the sin/cos pair the angle
// indexes, and once over that pair turned back through the matrix built so far.
//
// NON-OBVIOUS PARTS. The +0x34 halfword is set to -1 in the `beqz`'s DELAY SLOT, so it is written
// on both arms and the index store is the only thing that follows a non-zero head; the bucket
// lookup uses the UNTRUNCATED index while the moby keeps only its low half. An angle's first MVMVA
// takes the sin/cos pair one way round and its second the OTHER way round with the sin half
// negated, and the first two angles pack the two halves the other way round again — the matrix's
// halves are assembled from those partial products, not recomputed. The THIRD angle writes no
// control register at all: its four words go straight to the moby. v0 exits as the sin table and v1
// as the cos table, the last thing the body writes to either.
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
    // The two halves go back the OTHER way round: the Y half negated, the X half as it is.
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


// ── 0x80037F90 — count a moby timer down by the frame delta global: a0 is the timer, a1 its width
//     in bytes (4, 2 or 1), and each width is one copy of load, `slt` the signed 32-bit delta
//     against the timer, then either zero the timer and return 2, return 1 because it was already
//     zero, or subtract the delta and return 0. v1 exits holding the loaded timer — sign-extended
//     for `lh`, zero-extended for `lbu` — and it is filled from a2 in a branch delay slot, so it is
//     set on both arms. A width that is none of the three jumps straight out with v0 = 1 and v1
//     untouched, which is the value the decomp's missing return actually produces.

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

// Resets the moby at $a0 to its spawn defaults: 20 byte stores, 3 word stores and 1 halfword
// store, with no branch and no load. Retail body at 0x8003A720. Its last word store sits in the
// $jr $ra delay slot, so the specular/metal colour word is written on the same path as the rest.
// The function is void in the source sense but still clobbers the two registers the differential
// compares: $v0 leaves holding 0x7F, the sound-channel byte stored last, and $v1 holding 0xFF,
// the unset index value it reuses for the pod, sector and drop stores.
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
// The highest world or actor triangle below a point; it snapshots its caller's s0..s7, gp, sp, fp
// and ra into 0x3C017DD8, so the caller's sp, ra and s0 are behaviour.
constexpr std::uint32_t kFindGroundHeightBelow = 0x8004D5ECu;       // `jal` at 0x80038360
constexpr std::uint32_t kFindGroundHeightBelowReturn = 0x80038368u; // the `jal`'s own pc + 8

// ── 0x80038340 — the ground height under a moby, asked for from 1500 units above it.
//     a0 = &moby->m_Position, a1 = 4096. The z bump is STORED before the query runs — the store is
//     the `jal`'s delay slot — and the z the restore leaves is what v1 exits holding, reloaded from
//     memory rather than from the pre-call word, so a query that moved z is reflected. v0 is the
//     query's own return, which the body never touches again.
//     The frame, s0 and ra are behaviour, not bookkeeping: the query records its CALLER's s0..s7,
//     gp, sp, fp and ra into RAM at 0x3C017DD8 before it reads a0/a1, so an override that entered
//     it with the host's sp and ra wrote words nobody else writes. The body therefore stands up
//     retail's 0x18-byte frame, s0 = the moby, and ra = the instruction after the `jal` for the
//     duration of the call. The frame's own two stores are inside the 8 KiB of dead stack the
//     differential ignores, and the callee reads the caller's registers rather than the caller's
//     frame, so they are not reproduced.
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

// ── 0x80037E98 — the moby interpolation check, whose retail body is EMPTY: `jr $ra` at 0x80037E98
//     and a `nop` in its delay slot, and no third instruction anywhere in the function. It writes
//     no register and no memory, so v0 and v1 carry in whatever the caller left and the moby
//     pointer it is handed is never read.
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
