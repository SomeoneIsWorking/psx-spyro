---
id: 189
title: Two host-memory implementations read guest words for producers
status: open
symptom: two classes map guest addresses for in-between reads; a fix to one (KUSEG/KSEG0 canonicalization) has to be made twice
tags: render,structure,duplication
created: 2026-10-15
updated: 2026-10-15
---

## Gap
Reported by the Spyro 1 emitter pass. `game/render/field/guest_working_memory` is one owner; find the
other and fold it into it, so every producer reads guest words through one mapping.
