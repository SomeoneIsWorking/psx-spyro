# 0257 — The selector showed two panels for three titles

Status: fixed (2026-11-25, psxport 7f0b2db9 and 97c979ac). Owner: psxport `PickerSession`/`PickerPanes`
(`docs/plans/static-title-picker.md`).

## Observed
User report 2026-11-21 ("this is supposed to be split 3 way, not 2"): the selector showed Spyro 1 and Spyro 2 side
by side, with no panel for Spyro 3, although the bottom band named all three.

## Fix
The live selector was replaced by the static picker, which has one panel per available title. Each panel shows
the wordmark read from that title's own disc.

## Proof
`spyro_port` with no arguments, hidden, debug-server walk on 2026-11-27 (`scratch/picker3/`): three panels split by
two seams, Spyro the Dragon selected in colour, with Ripto's Rage and Year of the Dragon in grey. After
`select right`, Ripto's Rage is in colour between the other two. The log has one `panel art … from its disc` line for
each title.
