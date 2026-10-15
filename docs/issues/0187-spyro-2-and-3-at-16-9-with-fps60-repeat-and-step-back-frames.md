---
id: 187
title: Spyro 2 and 3 at 16:9 with fps60 repeat frames and step backwards
status: open
symptom: boot smoke at 16:9 with fps60 shows repeated presents (S2 721, S3 1028) and backward steps (S2 32, S3 2) in 3000 steps
tags: spyro2,spyro3,fps60,present
created: 2026-10-15
updated: 2026-10-15
---

## Observed
Hidden-window boot smoke, 16:9, fps60 on, 3000 steps (spyro1 Record pass, 2026-10-15). Spyro 1 on the
same run shows 2560 in-betweens and 0 repeats.

## Gap
Not traced. Next: find which present repeats and whether the in-between time or the field pair goes
backwards, then follow it to the owner in the present path.
