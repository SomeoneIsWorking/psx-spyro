# RenderWorldChunks final stream

What Spyro 1's world drawer `0x800258F0` actually hands the GPU, recorded for its future producer
(issue 0186). The native world producer and the `PSXPORT_WORLD_SCENE_ORACLE` capture that measured
this were deleted with the native path (2026-10-15).

- The final linked stream is not the allocation stream. RenderWorldChunks first allocates and links
  oversized near faces, then its adaptive stage `0x8002A0A0` replaces those parents with child
  chains, so allocated packets include faces the GPU never receives.
- Final paint order is descending OT bin, then descending AddPrim paint group, then ascending
  adaptive child suborder. Membership is decided by walking all 0x800 world-OT bins from high to low
  at the body's epilogue, keeping only packets in that call's packet-pool span.
- A final packet proves family, order, SXY, RGB, UV, semi-transparency, CLUT/tpage when observable,
  and the OT bin. It carries no vertex depth (for example the GT4 at `0x8018E55C`), and an opaque
  untextured packet makes no tpage observable.
- The moving retail corpus of 2026-08-22 (3,000 frames, 1,275 world calls: LQ, HQ direct, medium,
  near, adaptive, transition and edge cases) matched the native producer on every one of those
  fields.
