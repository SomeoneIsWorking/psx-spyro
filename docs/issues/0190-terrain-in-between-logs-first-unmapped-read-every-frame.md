---
id: 190
title: The terrain in-between logs "first unmapped read" on every in-between
status: open
symptom: the terrmem info line repeats once per composed in-between frame
tags: render,terrain,logging
created: 2026-10-15
updated: 2026-10-16
---

## Gap
A "first" line that repeats means the read it reports happens every in-between. Trace which address
the terrain producer reads outside its mapped blocks and map it, rather than silencing the line.

The "first" is per `HostMemory` (`guestReads_ == 0`), and every producer render of an in-between builds
one, so the line fires once per render, not once per in-between: 196928 lines in a 75 s Spyro 1 play
check (2026-10-16). Reads outside the mappings are the level's records and are expected.
