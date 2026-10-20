# 0203 — a state saved mid-call resumes the guest at the wrong PC

**Status:** open, needs a decision. **Owner:** `game/core/boot_prefix_frame_driver.cpp`
(`BootPrefixFrameDriver::call_` and `loopCall_`), against psxport's `runtime/psx/state/machine_state.cpp`.

## Reproduction
Spyro 3, hidden window, `PSXPORT_DEBUG=xa,cdcmd,cdirq,boot`
(`scratch/b2fix/roundtrip.py spyro3 rt3 xa,cdcmd,cdirq,boot 5`): reach the title, wait for gamestate 5
(the attract load), `state save`, run 120 fields, `state load`.

- At the save: `step 1541 phase=main-loop call_active=1 resume=0x80061584 turns=4`.
- At the load: `step 1661 call_active=1 resume=0x8002CB10 turns=2`.
- Next step: `resume=0x00000000`, then `Spyro 3 stopped: ... call 0x80055400 active=1 ... guest pc=0x00000000`
  and abort.

B2's Spyro 3 run (saved at step 6410 in the same load phase) instead lost every later CdRead completion,
and its Spyro 2 run hit a Lightrec fault once. Saves at the title, where the call resumes at the same
display wait each step (Spyro 2: `resume=0x80015944` at both ends), round-trip cleanly.

## Cause
The main-loop guest call spans host turns while the guest waits on the CD. The machine state restores the
CPU registers of the saved turn, but the driver's `call_` (pending, resume PC, turns) and `loopCall_` are
host state outside it, so the next `advance` resumes the live call's PC under the saved registers.

## Fix (choose one)
- Spyro declares a `NativeStatePort` carrying `loopCall_` and the call's resume state, with
  `ResumableGuestCall` gaining a save/restore of its pending call.
- Or `MachineState` refuses a save while a title reports a guest call in flight, and the REPL `state save`
  waits for the next step boundary with none.
