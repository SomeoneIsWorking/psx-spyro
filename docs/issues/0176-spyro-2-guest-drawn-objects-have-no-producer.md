---
id: 176
title: Spyro 2's guest-drawn objects have no producer
status: resolved
symptom: mobys, HUD, sky and particles are unkeyed; the census cannot rank their submitters
tags: spyro2,record,census
features: [F-FPS60]
created: 2026-10-08
updated: 2026-10-21
---

Census at shutdown on the 4:3 route: 977,048 primitives, 434,692 keyed (all terrain, 0x80023BB4),
3,644 unkeyed attributed, 538,712 span-miss, duplicated keys 0. The guest submitters below the
draw cannot be ranked: the driver's logic-frame clock (`OtAttr::beginLogicFrame` at the frame tail)
drops the packet spans between the step that builds a table and the step that walks it (issue
0177), so the walk finds no span. Without that clock the spans are never dropped and reused pool
words are charged to stale writers (93,097 unkeyed packets charged to the terrain producer, which
keys every packet it writes). The known guest drawers are the moby drawers (FUN_80044504,
FUN_80046FD8, close mobys FUN_800499D4), the HUD FUN_80053E78, the sky and the particles. Each is one
guest call that loops over its own list (re-frontier `guest.moby-provenance`), so a key per object
needs that drawer native.

Since an in-between with any unkeyed primitive is shown whole (issue 0196), these drawers decide
whether Spyro 2 interpolates at all. Spyro 1 reached keyed-only gameplay frames by porting each
drawer the census named and keying it per object (issue 0186); the same applies here.

## Resolution (2026-10-21)
Cause: every Spyro 2 primitive but the terrain came from guest code inside the draw dispatcher
`0x800155A0` or from logic before it, with no emission scope, so `composeFrame` showed every
gameplay frame whole.

Fix: the field is one producer, `spyro::field_redraw` (re-frontier `spyro2.field-redraw`). The
dispatcher's override reads the camera, the moby records and Spyro, runs the dispatcher once more
less the terrain flag as a `psx::present::GuestRedraw` with those inputs half way from the last
field's, keeps the primitives it linked into the bins and the overlay pair, and then makes the real
call. Ownership sits at the flatten `0x8001B2A8`, where the bins are whole even when the real call
outlives the host turn: it keys every unkeyed packet linked before the letterbox `0x8004D604` (the
dispatcher's last call) returned and saves the redraw as the field's state. Three faults found on the
way: the bin count was 512 where the table holds 0x580 pairs, so the sky (linked at the mark, bin
0x201) was neither cleared nor harvested; the redraw cleared the bins first, so the conversation
box and text that logic links before the dispatcher vanished from in-betweens; and the claim took
packets linked after the dispatcher, which the redraw never draws.

Proof (fps60, 16:9, hidden window, `PSXPORT_PRODUCERS=1`, 40 presents while turning after `warp`
10, 11, 22): census unkeyed 32 and span-miss 123 at gameplay arrival and at shutdown alike (all
boot), keyed without render 0, no redraw warning; adjacent differences even (10: 11.5..14.8, 11:
4.3..9.2, 22: 2.3..5.9) against 0/19 alternating before; 81..95% of each in-between's changing pixels
lie between its neighbours; the Glimmer conversation box present in every present, stepping with
logic; HUD anchored on in-betweens. Captures `scratch/interp2/f10`, `f11`, `f22`, `h10`.
Review (2026-10-21): the redraw also moves the effects pass's records (`spyro::effect_pools`) and
each animated mesh's keyframe pose (`spyro::keyframe_blend`); before, those drew the next field's
state at the in-between (issue 0196 has the proof).
Left: the attract demo's DEMO MODE text (linked after the dispatcher) shows its field whole; level
21 runs logic at 20 fps and gets one in-between per three fields.
