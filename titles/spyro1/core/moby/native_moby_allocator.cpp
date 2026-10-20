#include "spyro1/core/moby/native_moby_allocator.h"

#include "core/native_execution.h"
#include "spyro1/core/guest_globals.h"

#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kMobyCollisionChain;

// g_DynMobyCount (moby.h:507), bumped once per call before either pool is touched.
constexpr std::uint32_t kDynMobyCount = 0x800756A4u;
// g_MobyAllocPtr walks up through 0x58-byte slots; +0x48 is both link byte and alive flag.
constexpr std::uint32_t kMobyAllocCursor = 0x8007573Cu;
constexpr std::uint32_t kMobySlotStride = 0x58u;
constexpr std::uint32_t kMobyLinkByte = 0x48u;
// g_PropsAllocPtr walks down through 24-byte records; a record's link byte sits just below it.
constexpr std::uint32_t kPropsAllocCursor = 0x80075930u;
constexpr std::uint32_t kPropsStride = 0x18u;
constexpr std::uint32_t kPropsLinkByte = 1u;
constexpr std::uint32_t kMobyPropsPointer = 0x00u;
// Link bytes compare sign-extended against -1, so only 0xFF skips the walk.
constexpr std::int32_t kTakenLinkMarker = -1;

// 0x800524C4: each walk steps its cursor in the delay slot and undoes it on exit.
void allocateDynamicMoby(Core *c) {
  c->mem_w32(kDynMobyCount, c->mem_r32(kDynMobyCount) + 1u);

  const std::uint32_t moby = c->mem_r32(kMobyAllocCursor);
  const std::int32_t link = static_cast<std::int32_t>(c->mem_r8s(moby + kMobyLinkByte));
  c->mem_w8(moby + kMobyLinkByte, 0u);
  std::uint32_t slot = moby + kMobySlotStride;
  if (link == kTakenLinkMarker) {
    c->mem_w8(slot + kMobyLinkByte, static_cast<std::uint8_t>(kTakenLinkMarker));
  } else {
    while (static_cast<std::int32_t>(c->mem_r8s(slot + kMobyLinkByte)) >= 0) {
      slot += kMobySlotStride;
    }
  }
  c->mem_w32(kMobyAllocCursor, slot);

  const std::uint32_t head = c->mem_r32(kPropsAllocCursor);
  std::int32_t propsLink = static_cast<std::int32_t>(c->mem_r8s(head - kPropsLinkByte));
  c->mem_w8(head - kPropsLinkByte, 0u);
  std::uint32_t props = head - kPropsStride;
  if (propsLink == kTakenLinkMarker) {
    c->mem_w8(props - kPropsLinkByte, static_cast<std::uint8_t>(kTakenLinkMarker));
  } else {
    for (;;) {
      propsLink = static_cast<std::int32_t>(c->mem_r8s(props - kPropsLinkByte));
      props -= kPropsStride;
      if (propsLink < 0) {
        break;
      }
    }
    props += kPropsStride;
  }
  c->r[3] = head - kPropsStride;
  c->mem_w32(moby + kMobyPropsPointer, c->r[3]);
  c->mem_w32(kPropsAllocCursor, props);
  c->r[2] = moby;
  c->r[1] = kPropsAllocCursor;
  c->r[4] = props;
  c->r[5] = static_cast<std::uint32_t>(propsLink);
}


// The first dynamic moby; one below it is only marked.
constexpr std::uint32_t kDynMobys = 0x80075890u;
constexpr std::uint32_t kMobyChainLink = 0x04u;
constexpr std::uint32_t kMobyCollisionRegion = 0x34u;
// The dead-moby marks, sign-extended as the body holds them; `sb` keeps the low byte.
constexpr std::uint32_t kMarkBelowPool = 0xFFFFFFFDu;
constexpr std::uint32_t kMarkReleased = 0xFFFFFFFEu;
constexpr std::uint32_t kMarkRunBoundary = 0xFFFFFFFFu;

// 0x80052568: every mark test is a sign-extended byte read in a delay slot that steps the walk.
void releaseMoby(Core *c) {
  const std::uint32_t moby = c->r[4];
  const std::uint32_t region = static_cast<std::uint32_t>(c->mem_r16s(moby + kMobyCollisionRegion));
  if (static_cast<std::int32_t>(region) >= 0) {
    std::uint32_t slot = c->mem_r32(kMobyCollisionChain) + (region << 2);
    while (c->mem_r32(slot) != moby) {
      slot = c->mem_r32(slot) + 4u;
    }
    c->mem_w32(slot, c->mem_r32(moby + kMobyChainLink));
  }

  c->r[2] = kMarkBelowPool;
  if (static_cast<std::int32_t>(c->mem_r32(kDynMobys) - moby) > 0) {
    c->mem_w8(moby + kMobyLinkByte, static_cast<std::uint8_t>(kMarkBelowPool));
    return;
  }
  c->r[2] = c->mem_r32(kDynMobyCount) - 1u;
  c->mem_w32(kDynMobyCount, c->r[2]);

  std::uint32_t pool = moby;
  c->r[2] = static_cast<std::uint32_t>(c->mem_r8s(moby + kMobySlotStride + kMobyLinkByte));
  c->r[3] = kMarkReleased;
  if (c->r[2] != kMarkRunBoundary) {
    c->mem_w8(pool + kMobyLinkByte, c->r[3]);
  } else {
    pool = moby - kMobySlotStride;
    for (;;) {
      c->r[2] = static_cast<std::uint32_t>(c->mem_r8s(pool + kMobyLinkByte));
      c->r[3] = kMarkReleased;
      pool -= kMobySlotStride;
      if (c->r[2] != kMarkReleased) {
        break;
      }
    }
    pool += kMobySlotStride;
    pool += kMobySlotStride;
    c->r[3] = kMarkRunBoundary;
    c->mem_w8(pool + kMobyLinkByte, c->r[3]);
  }
  c->r[3] = c->mem_r32(kMobyAllocCursor);
  if (static_cast<std::int32_t>(c->r[3] - pool) >= 0) {
    c->mem_w32(kMobyAllocCursor, pool);
  }

  const std::uint32_t props = c->mem_r32(moby);
  std::uint32_t entry = props + kPropsStride;
  c->r[2] = static_cast<std::uint32_t>(c->mem_r8s(props - 1u));
  c->r[3] = kMarkReleased;
  if (c->r[2] != kMarkRunBoundary) {
    c->mem_w8(entry - 1u, c->r[3]);
  } else {
    entry += kPropsStride;
    for (;;) {
      c->r[2] = static_cast<std::uint32_t>(c->mem_r8s(entry - 1u));
      c->r[3] = kMarkReleased;
      entry += kPropsStride;
      if (c->r[2] != kMarkReleased) {
        break;
      }
    }
    entry -= kPropsStride;
    entry -= kPropsStride;
    c->r[3] = kMarkRunBoundary;
    c->mem_w8(entry - 1u, c->r[3]);
  }
  c->r[2] = kPropsAllocCursor;
  c->r[3] = c->mem_r32(kPropsAllocCursor) - entry;
  if (static_cast<std::int32_t>(c->r[3]) <= 0) {
    c->mem_w32(kPropsAllocCursor, entry);
  }
}

} // namespace

void registerMobyAllocatorOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x80052568u, "release_moby", releaseMoby);
  psx::cpu::installNativeOverride(core, 0x800524C4u, "allocate_dynamic_moby", allocateDynamicMoby);
}

} // namespace spyro1::native
