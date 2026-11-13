// Small engine utilities this port owns outright: a set-and-return-previous global, a 2D distance
// approximation, and the display-list link primitive. Every register a body leaves is part of the
// contract, and delay slots decide the exit state: dl_link's pointer store is a branch delay slot,
// so the new node becomes the list head on BOTH paths.
#include "core.h"
#include "core/native_execution.h"
#include "render/overlay_pair.h"
#include "spyro1/core/execution/native_owners.h"
#include "spyro1/render/crop_facts.h"
#include "spyro1/render/inventory_band.h"

namespace {

// 0x8006276C (strlen) and 0x80067614 (a set-and-return-previous global) are deliberately not
// installed: no headless run calls either, so neither can be differentially verified.

// 0x80063C30 — set a global, return its PREVIOUS value.
//     lui v0,0x8007 ; lw v0,0x4E38(v0) ; lui at,0x8007 ; sw a0,0x4E38(at) ; jr ra
// The load of the old value precedes the store, so this is a swap rather than a plain setter. The
// address is formed TWICE, by two separate luis, so $at exits holding 0x80070000 — the bare lui
// result, not the full address.
constexpr uint32_t kG3C30 = 0x80074E38u;
void setg_3c30_native(Core *c) {
  c->r[2] = c->mem_r32(kG3C30); // v0 = old
  c->mem_w32(kG3C30, c->r[4]);
  c->r[1] = 0x80070000u; // at — the lui, not the address
}

// 0x80017990 — approximate 2D distance between two points.
//     dx = |[a1+0] - [a0+0]|, dy = |[a1+4] - [a0+4]|,  result = max + (3*min >> 3)
// The classic octagonal approximation: exact on the axes, no square root. The shift is srl
// (LOGICAL), safe only because both values are already absolute. at exits as 2*min on both arms:
// it holds dx-dy at the comparison, then each arm overwrites it with the doubled minimum.
void dist2d_native(Core *c) {
  const uint32_t a0 = c->r[4], a1 = c->r[5];
  const uint32_t x0 = c->mem_r32(a0 + 0), y0 = c->mem_r32(a0 + 4);
  uint32_t dx = c->mem_r32(a1 + 0) - x0;
  uint32_t dy = c->mem_r32(a1 + 4) - y0;
  if ((int32_t)dx < 0) {
    dx = 0u - dx; // `sub a2,zero,a2`, so INT_MIN maps to itself
  }
  if ((int32_t)dy < 0) {
    dy = 0u - dy;
  }
  uint32_t at;
  if ((int32_t)(dx - dy) >= 0) {
    at = dy << 1;
    dy = (dy + at) >> 3;
  } // dx is the max
  else {
    at = dx << 1;
    dx = (dx + at) >> 3;
  } // dy is the max
  c->r[1] = at;      // at  — 2*min
  c->r[6] = dx;      // a2
  c->r[7] = dy;      // a3
  c->r[2] = dx + dy; // v0
}

// 0x800168DC, the overlay pair's link (render/overlay_pair.h).
constexpr spyro::overlay_pair::Facts kOverlayLink{.entry = 0x800168DCu,
                                                  .overrideName = "dllink",
                                                  .pairWord = 0x8007581Cu,
                                                  .wholeScreenReturns = spyro1::kWholeScreenLinks};
void dl_link_native(Core *c) {
  const uint32_t node = c->r[4];
  spyro::overlay_pair::link(*c, kOverlayLink);
  spyro1::inventory_band::linked(*c, node);
}

} // namespace

void spyro1::registerNativeUtil(Core &core) {
  psx::cpu::installNativeOverride(core, 0x80063C30u, "setg3c30", setg_3c30_native);
  psx::cpu::installNativeOverride(core, 0x80017990u, "dist2d", dist2d_native);
  spyro::overlay_pair::install(core, kOverlayLink, dl_link_native);
}
