# 0203 — a state saved mid-call resumes the guest at the wrong PC

**Status:** fixed. **Owner:** `game/core/boot_prefix_frame_driver.cpp` (`BootPrefixFrameDriver`), with
psxport's `runtime/cpu/resumable_guest_call.cpp` (`ResumableGuestCall::save`/`load`).

## Reproduction
Spyro 3, hidden window, `PSXPORT_DEBUG=xa,cdcmd,cdirq,boot`
(`scratch/0203/roundtrip.py spyro3 <tag> xa,cdcmd,cdirq,boot 5`): reach the title, wait for gamestate 5
(the attract load), `state save`, run 120 fields, `state load`.

- At the save: `step 1541 phase=main-loop call_active=1 resume=0x80061584 turns=4`.
- At the load: `step 1661 call_active=1 resume=0x8002CB10 turns=2`.
- Next step: `resume=0x00000000`, then `Spyro 3 stopped: ... call 0x80055400 active=1 ... guest pc=0x00000000`
  and abort.

B2's Spyro 3 run (saved at step 6410 in the same load phase) instead lost every later CdRead completion,
and its Spyro 2 run hit a Lightrec fault once. Saves at the title, where the call resumes at the same
display wait each step, round-tripped cleanly.

## Cause
`BootPrefixFrameDriver::stepMainLoop` resumes `call_` across steps, and `ResumableGuestCall::advance`
puts back the register file it kept at the last suspension and continues at its resume PC. Neither the
call (pending, resume PC, kept registers, turns) nor the driver's `loopCall_` and phase were in the
machine state, so after a load the next step ran the live call's registers and PC over the saved RAM.

## Fix
The call in flight is machine state. `ResumableGuestCall` saves and loads its whole call (owner, entry,
return, resume PC, cap, turns, cycles, kept registers, pending); `BootPrefixFrameDriver` is S2/S3's
`NativeStatePort` (section `spyro.frame-driver` v1: phase, loop call, boot stall and step count, the
call), staged in `load` and adopted in `restored`. Its duplicate turn/cycle counters are gone; the step
log reads the call's own. Spyro 1 has no call crossing a step and keeps no title section. States written
before this fix carry no title section and are refused for S2/S3 by name.

## Proof (2026-10-21, `scratch/0203/run/`)
- Spyro 3 (`spyro3-final.log`): saved at step 1541 `resume=0x80061584 turns=4`; after the load step 1662
  `resume=0x800504F0 turns=5`, the saved call continuing. Gamestates after the load 5, 3, 0 (playing),
  5; `spyro3-final-end.png` is the demo's "Exiting Demo Mode"; 27 CD data-ready completions, exit 0.
- Spyro 2 (`spyro2-final.log`): saved at step 1691 `resume=0x8004D918 turns=8`, live call at the load
  `resume=0x80019628 turns=96`; after the load `resume=0x8004D918 turns=9`. Gamestates 5, 3, 0, ...;
  `spyro2-final-t060.png` and `-t100.png` show the demo gameplay; `faults=0`, exit 0. The control run
  without the load (`spyro2-ctl2-*`) shows the same gamestate sequence and captures.
- Spyro 1 mid-XA at the title (`spyro1-final.log`): load accepted, title stays at gamestate 13, XA audio
  continues (`xadump` 4.00 s, peak 19364), `spyro1-final-end.png` shows the title.
