---
id: 190
title: The terrain in-between logs "first unmapped read" on every in-between
status: open
symptom: the terrmem info line repeats once per composed in-between frame
tags: render,terrain,logging
created: 2026-10-15
updated: 2026-10-15
---

## Gap
A "first" line that repeats means the read it reports happens every in-between. Trace which address
the terrain producer reads outside its mapped blocks and map it, rather than silencing the line.
