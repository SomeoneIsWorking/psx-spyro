---
id: 183
title: Spyro 2 and 3 ran two guest fields per host field, so audio played at twice its rate
status: resolved
symptom: Spyro 2 audio 60,000-88,000 samples/s, 165-240 SPU fields per 2 s, 44-119 drops per 2 s, queue at its cap, a RESYNC every pacer cycle
tags: spyro2,spyro3,audio,pacing,fields,irq
created: 2026-10-13
updated: 2026-10-13
---

## Observed
Player build, mapped window, `pick spyro2` and `spyro_port scratch/assets/spyro2/SCUS_944.25`:
`audio_rate` 80,000-90,000 samples/s, ~240 SPU fields per 2 s, ~120 drops per 2 s, backlog at the
cap. The guest's VSync counter `0x80066618` advanced 1 per product step while the field owner
delivered 2 (`frame-draw` + `frame-tail`), and the pacer waited 1 field per step.

## Cause
Two defects in `game/core/field_owner.cpp`, cancelling for game speed but not for audio:

- `FieldOwner::dispatchCallbacks` returned without delivering anything when the title has no
  host-dispatched vblank root (Spyro 2 and 3 take VBlank through their own interrupt chain). The
  `frame-tail` field follows the draw's wait field with no guest execution between, so the two VBlank
  edges latched into one and the guest counted half the fields: the limiter at `0x80015900`
  (`VSync(-1) - last < 2`) spanned four delivered fields per logic frame.
- `FieldOwner::deliver` paced each present as one field (`request.pace ? 1 : 0`) although every step
  delivers two; a present deferred by the temporal product (`commitUnpresented`) was not paced at all,
  so Spyro 3's boot prefix ran at ~220 fields/s.

Not involved: psxport 7eaf01a0 (CD due-sector catch-up, GetlocP, CdlLastCom, XA push pacing). The
drive and XA follow emulated time, which the field owner advanced at twice wall time.

## Fix
`FieldOwner::dispatchCallbacks` runs `Hle::irqPoll` for a pending, enabled VBlank when no root or
fallback callback exists. `FieldOwner::takePacedFields` hands a paced field the count delivered since
the previous paced one, and a deferred paced field waits it out through `FramePacer::waitPresentedFields`.
Reaches Spyro 2 and 3 (both on `BootPrefixFrameDriver`); Spyro 1 has a guest root and presents
through `FrameRenderer`, so only its pre-root boot fields take the new interrupt path.

Tests: `tests/test_boot_prefix_frame_driver.cpp` `each_field_hands_its_vblank_to_the_guest_chain`,
`a_paced_field_waits_for_every_field_since_the_last`, `a_deferred_paced_field_still_waits` (all three
failed before).

## Measured after
| title | path | load | guest VBlank/s | presents/s real+in-between | audio samples/s | drops after boot |
|---|---|---|---|---|---|---|
| Spyro 2 | picker, 120 s | 1.0-1.8 | 60.0 | 30 + 30 | 44,100 | 0 |
| Spyro 3 | direct, 45 s | 2.0-3.1 | 59.9 | 30 + 30 | 44,100 | 0 |
| Spyro 1 | picker + user pad, 70 s | 2.0-3.8 | — | — | 44,100 | 0 |

## Open
- The boot prefix delivered every display wait of a call within one step; fixed in 0184.
- A constant fs/8 (5,512 Hz) tone on a ~1,400 DC offset is in every title's capture from the first
  field; it is unchanged by this fix (guest-time captures match sample for sample).
- Spyro 2's title screen hitches for ~1 s around 47 s (RESYNCs, 39,000-42,000 samples/s windows).
