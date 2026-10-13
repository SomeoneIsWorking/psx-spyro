---
id: 184
title: Spyro 2 and 3 boot-prefix waits reached the host in one burst
status: resolved
symptom: Spyro 2 boot after the pick, 219 audio fields in 3.7 s then a 2 s stall, 104-169 fields dropped against the queue limit, three queue underruns
tags: spyro2,spyro3,audio,pacing,boot
created: 2026-10-13
updated: 2026-10-13
---

## Observed
Player build, mapped window, `pick spyro2` (`scratch/s1-play/vrun.py`, load 1.8-3.5): the first
`audio_rate` window after the pick read `219 calls/3.7s, drops=104`, the next two 30 and 35 drops, and
the queue emptied three times in the following 3 s while the boot picture held.

## Cause
`game/core/boot_prefix_frame_driver.cpp:BootPrefixFrameDriver::runCall` delivered each boot display
wait unpaced, and a boot call answers every wait with another up to its field bound, so one step handed
`FieldOwner::deliver` (and `SpuAudio::frame`) up to ~120 fields at once; the step's present then waited
all of them out as one interval.

## Fix
Boot-phase waits are delivered paced, and `FieldOwner::deliver` waits a paced field that presents nothing
through `FramePacer::waitPresentedFields`. Main-loop draw waits stay unpaced, since their present
follows at once and `Fps60` splits the step's two fields across its two presents. Reaches Spyro 2 and 3
(`BootPrefixFrameDriver`); Spyro 1 delivers no unpresented paced field.

Tests: `tests/test_boot_prefix_frame_driver.cpp` `a_paced_wait_field_waits_without_presenting` and the
`paces() == fields()` check in `boot_prefix_that_never_returns_is_bounded` (both failed before).

## Measured after
Spyro 2, same route, load 1.4-2.0: 120 calls per 2 s from the pick on, 0 drops, 0 underruns in the boot.
Spyro 3 boot prefix returns after 665 steps and 884 fields as before.
