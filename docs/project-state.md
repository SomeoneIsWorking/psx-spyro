# Project state

Factual capability coverage for the Spyro trilogy's native/Lightrec execution and presentation.
Goal G001 is three products, so Spyro 2 and Spyro 3 carry their own rows here even though neither is
being implemented yet; a capability nobody has started is `missing`, not absent from the inventory.
Atomic work lives in
`docs/issues/`, ownership and placement in `docs/codemap.md`, and the ordered binary-evidence chain
in `docs/re-frontier.md`.

## Comparison baseline

The comparison baseline is the original USA PSX releases as run in a faithful emulator. The
repository also has a retired intermediate baseline: native-enhanced executables whose remaining
guest code was emitted offline as C. Evidence from that path remains valid only for the exact binary
behavior or native owner it observed; it does not prove that the native/Lightrec product exists.

| ID | Capability / observable outcome | State | Dependencies | Goals |
|---|---|---|---|---|
| S001 | The verified Spyro 1 executable reaches stage 13's title overlay under the recorded native owners | partial | — | G001 |
| S002 | Stage-13 title modes 0 and 1 are presented | partial — on Record the guest's title overlay draws them and recordcheck at t=1 matches through the boot route; the native title-menu owner was deleted 2026-10-15 | S001 | G003 |
| S003 | Stage-13 title mode 2 presents the three-slot save screen | partial — on Record the guest's overlay draws it; the 900-field mode-2 route has not been rerun on Record (it was verified on the deleted native owner) | S002 | G003 |
| S004 | Spyro 1 boot and gameplay advance under a title-owned frame/field scheduler | partial — since 2026-10-15 the main loop runs GamestateDraw `0x8001ED5C` through its own display tail: each `VSync(0)` of its pace loop is one field delivered by `spyro1::FrameDraw`, and the frame is presented once after its DrawOTag (`FieldScheduler::presentFrame`); the boot logos still run on `spyro1::BootSequence` | S001 | G002, G003 |
| S005 | Spyro 1 presents on the Record path | partial — since 2026-10-15 `Spyro1Runtime` selects `RenderPath::Record` with `interpolates()`; the device draws the guest's own ordering table, so draw order is retail OT order. `PSXPORT_DEBUG=recordcheck`, 1x 4:3, 3000 steps to the attract demo's gameplay (gamestate 0): 2566 of 2566 checked records `mismatched=0` (the 24bpp boot logos are not record presents). Its producers are the fullscreen tint 0x800190D4 and cinematic letterbox 0x80018F30 (`spyro1::ScreenOverlays`); the same run composes the tint at t=1 with mismatched=0 and the census shows keyed 44, all under a producer with a render. The terrain, sprite queue, Spyro, moby emitters, sky, drop shadow, glows and particles are producers too (S020); portals and the remaining effects are the guest's packets as drawn, so their in-betweens repeat the shown record (issue 0186) | S002, S004 | G003 |
| S006 | Spyro 2 has identity-derived executable facts and a title-local native boot owner through the pre-display boundary | partial — the finite three-field bootstrap and its `0x80011B1C` stop are DELETED; the retail boot prefix `0x80011E9C` executes through Lightrec and, since issue 0156, RETURNS: the loader's modules are published as images (framework `stockCdReadLanded`, psxport `guest_code_module.*`), the boot prefix returns after 43 steps and 133 fields with instantaneous CD reads (359 steps and 606 fields once reads take the drive's time, issue 0164), and the per-frame update `0x8001B140`/draw `0x800156FC` then run 784 product steps / 1,500 fields with 0 faults (`tools/boot_run.py --title spyro2 --frames 784`, fence steps); the loader's CD completion is fixed (psxport `28682cde`, issue 0143). Nothing is claimed about what those frames draw, and zero interpreter fallback is still refused by a Lightrec self-modifying-block false positive at `0x8005FFFC` (37 blocks, issue 0092 section 4) | — | G001 **MEASURED 2026-10-01 (commits a8781a3, 02a9514, 3bbaf41, 9a7bb73; issues 0159-0162): the title screen is PRESENTED.** Captures at steps 300..2400 (`PSXPORT_PRESENT_SHOT_AT`, headless): logo sign at 300 (555,363 of 691,200 pixels non-black), Spyro flies in at 600 (621,978), the "Spyro Ripto's Rage" logo with "press start" from step 900 (621,534; 621,363 at 2400). Blockers cleared: the frame driver ran one call per step (0162), negative VSync answered the HBlank timer instead of `[0x80066618]` (0160), retail CdControlB `0x80058994` waited out a 0x3C0-vblank deadline before each XA Setmode (0161), native CdInit removed (0159). Not compared pixel-for-pixel against the full-console oracle; fields-per-step is an assumption (2). |
| S007 | Spyro 1 accepts held digital input and moves the player after the New Game field handoff | partial | S004 | G001, G003 |
| S008 | psxport executes remaining Spyro guest code through a per-Core Lightrec runtime with bounded, accounted fallback | partial | — | G002, G004 |
| S009 | Spyro 1 reaches both stage-13 800/900 discriminators through Lightrec and native frame ownership | verified | S004, S008 | G001, G002 |
| S010 | The generated world-body include is replaced by resumable runtime guest execution | partial | S008, S009 | G002 |
| S011 | Representative Spyro 1 gameplay conforms on each released host through native/Lightrec execution | missing | S005, S007, S010 | G001, G002, G003, G004 |
| S012 | The frozen launcher builds and runs the native/Lightrec product without offline guest translation | partial | S008, S011 | G004 |
| S013 | Windows CI produces an installable release with matching-host runtime checks | missing | S008, S018 | G004 |
| S014 | macOS CI produces a runnable `.app` release with matching-architecture runtime checks | missing | S008, S018 | G004 |
| S015 | Linux CI produces a runnable AppImage release with package-install and runtime checks | missing | S008, S018 | G004 |
| S016 | Android CI produces an APK with ARM64 dynarec execution and authored touch controls | missing | S008, S018 | G004 |
| S017 | A WASM gameplay build is released through CI and deployed on GitHub Pages | missing | S008, S018 | G004 |
| S018 | Packaged first launch selects, validates and persists user-supplied game files without a terminal | missing | S001 | G004 |
| S019 | Widescreen renders additional horizontal scene coverage without stretching the original image: every horizontal cull or screen-rect limit the title owns is overridden natively so the margins show what the view would, with guest memory untouched | partial — on Record (2026-10-15) the canvas widens (`SpyroRuntime::guestWidescreenProjection`, 684x240 at 16:9); S1's terrain, moby list planes, moby emitters, sprite queue, sky, glows and particles cull on the widened window, leaving the seen and fade bytes, moby drawn byte +0x51, sector marks and sector animation as retail's 4:3 view writes them; its portal bodies clip at 512 (issue 0186) | S005 | G003 |
| S030 | Spyro 1 widescreen anchors the UI: edge HUD elements sit at the widened edges, centred elements stay centred, nothing stretches | partial — the HUD icon and text records and the life-orb and egg quads take their edge class through `spyro::ui_anchor` (`spyro1::hud_anchor`, `tests/test_spyro1_actors.cpp`); 16:9 captures show gems at the left edge, dragons centred, lives at the right edge. The life orbs were not on screen in the capture, and the transition fade still covers only the 4:3 columns | S019 | G003 |
| S020 | Spyro 1 presents interpolated 60fps | partial — producers are the fullscreen tint and the letterbox (`tests/test_spyro1_screen_overlays.cpp`), the terrain (`tests/test_spyro1_terrain.cpp`; override differential 4661/4661, 0 mismatches), the sprite queue and Spyro (`tests/test_spyro1_actors.cpp`; differentials 2744/2744 and 2739/2740, 1 incomparable), the moby emitters (`tests/test_spyro1_actors.cpp`; differentials 4792/4792 and 1972/1972), the sky, drop shadow, glows and particles (`tests/test_spyro1_effects.cpp`; differentials 4792/4792 and 1972/1972 each); census 7787825 of 7823787 primitives keyed. Portals and the remaining effects repeat the shown record (issue 0186) | S004, S005 | G003 |
| S021 | Touch-enabled releases provide an authored SVG control interface | missing | S018 | G004 |
| S022 | Spyro 1 streams its XA music through the shared CD/XA owner | partial — **MEASURED 2026-10-13 (issue 0182, psxport 0153-0155), player build in a mapped window with the user's pad:** the title stream decodes 238 of 238 sectors at -0.01% pitch (was 97, ~40% of the time audible), and the level music resumes at LBA 60000/60099 after the flyby skip (was LBA 0, silent). Still `partial`: not compared against the console, and the host queue holds ~200-290 ms of audio latency | S008 | G002 |
| S023 | Spyro 3 has identity-derived executable facts and a title-local native boot owner through the pre-display boundary | partial — every S023 fact is now confirmed against `SCUS_944.67`'s own crt0 and library bytes (entry `0x80059444`, bss `0x8006C4F4..0x800742D0`, stack top `0x8006C3E4` with bias `-8`, GP `0x8006C3B0`, game main `0x8001200C`, libetc VSync `0x8005956C` on counter `0x1F801110`, `SetGeomOffset 0x8005D35C`, `CdInit 0x8005DB1C`/`CdRead 0x8005D96C`/`CdSync 0x8005E074`/`CdCommand 0x8005E0BC`/`CdReadyCallback 0x8005DB08`), and the boot prefix `0x8002AB38` now **executes** through Lightrec on a real disc: 484 fields, 481 product steps, 481 presentation fences, 23,373,822 executed blocks, 135,585,657 instructions, 429 translated blocks, 0 faults, 0 interpreter fallback; that run stopped at `0x800504F0` inside the CD loader for a read completion the stock `CdRead` never raised (psxport `28682cde` fixed it), then at the loaded module's entry `0x80074DEC` for want of a published image (issue 0156 fixed it); the boot prefix now RETURNS (336 steps and 546 fields with instantaneous CD reads; 669 steps and 887 fields once reads take the drive's time, issue 0164), with 13 loader reads published as images. Guest libetc VSync `0x8005956C` **provably never executed**: 6 of 6 of its store instructions, 0 executions across 135,585,657 JIT instructions, against a control store that did execute in the same run. Still `partial`, not `verified`: `game main 0x8001200C` is never dispatched (the port drives the retail per-frame update `0x80055400` and draw `0x8001E638` directly), and the first main-loop draw's fault (`0x8001C4EC`) is fixed (issue 0158, psxport `96462c8e`); the run now reaches the 1,500-field cap with 0 faults (S025) | S008 | G001 **MEASURED 2026-10-01 (issues 0159, 0160, 0162): the title screen is PRESENTED.** Insomniac logo at step 300 (128,277 of 691,200 non-black), sign/egg/ninja at 600-900 (595,485-601,554), "Spyro Year of the Dragon" with "PRESS START" at 1200-1800 (621,849-622,011), then the attract demo "Entering Demo Mode" at 2400 (509,760). 0 `intr timeout`, 0 faults in 4500 fields after the native CdInit was removed so retail `CD_init` registers libcd's interrupt handler (0159). One "CD timeout ... CdlReset" from boot-time `CD_init` remains: the controller emulation does not answer the init reset. |
| S024 | Spyro 2 reaches representative gameplay through native/Lightrec execution | partial — **MEASURED 2026-10-01 (psxport `s23-play` `2bb8d2f6`, issues 0166, 0167): Spyro 2 reaches playable gameplay from the title and the player moves.** `tools/title_route.py --title spyro2`: Start at the title, then Cross/Start by observed state, reaches the playing state after 3340 fields on a blank card, 8 of 8 runs (3190 on a card a previous run had written; the route is deterministic only given its start state, issue 0169; states 0 11 5 6 5 6 5 3 0), 0 faults; the player triple `0x80067EE4` (x, y, z) went (68710, 33167, 9831) to (69350, 32710, 9831) under 60 fields of Down, and a 16-field Cross raised z 9831 to 10282. Captures `scratch/play/spyro2r/{settled,moved}.ppm` were opened: Spyro on grass with Sparx and a brown character. Blocker fixed at its cause: a native Pause never stopped the CD controller (issue 0166). **MEASURED 2026-10-01 (issue 0168, branches `s2-fix`/psxport `s2-lr`, unpushed): 0 fallback blocks, no allowance.** The `0x8005FFFC` false positive was `shared/lightrec` comparing a store's base register (`lui $at,0x8006`) instead of its resolved address; fixed in `4696481` (contract test red 6/15 on the old rule, 15/15 on the new), pinned by psxport `41373bc0`: gameplay at field 3190 on the written card (3340 on a blank one), `fallback_blocks=0`, 662,007,092 instructions. The first conversation draws its text (`tools/title_conversation.py`: 30 captures, 0 glyph pixels while the box opens, 2879 of 34,854 body pixels at the last; the early 'no glyphs' was the box before the typewriter reveal). The arrival level is read from guest words: Glimmer, id 11. Still `partial`: no oracle comparison; the conversation capture ends mid-speech; the framework commits are local and unpushed, so a checkout at the pushed pin does not yet reach gameplay | S006, S008 | G001, G002 **MEASURED 2026-10-01 (commits a8781a3, 02a9514, 3bbaf41, 9a7bb73; issues 0159-0162): the title screen is PRESENTED.** Captures at steps 300..2400 (`PSXPORT_PRESENT_SHOT_AT`, headless): logo sign at 300 (555,363 of 691,200 pixels non-black), Spyro flies in at 600 (621,978), the "Spyro Ripto's Rage" logo with "press start" from step 900 (621,534; 621,363 at 2400). Blockers cleared: the frame driver ran one call per step (0162), negative VSync answered the HBlank timer instead of `[0x80066618]` (0160), retail CdControlB `0x80058994` waited out a 0x3C0-vblank deadline before each XA Setmode (0161), native CdInit removed (0159). Not compared pixel-for-pixel against the full-console oracle; fields-per-step is an assumption (2). |
| S025 | Spyro 3 reaches representative gameplay through native/Lightrec execution | partial — **MEASURED 2026-10-01 (psxport `s23-play` `2bb8d2f6`, issues 0164, 0165, 0167): Spyro 3 reaches playable gameplay from the title and the player moves.** `tools/title_route.py --title spyro3`: Start, Cross/Start by observed state through two real cutscenes and three loads, reaches the playing state after 4890 fields on a blank card, 8 of 8 runs (4740 on a card a previous run had written; issue 0169; states 0 11 5 6 5 6 5 6 5 3 0), 0 faults, 0 fallback; the player triple `0x8006E020` went (39414, 13687, 18388) to (39421, 15359, 17929) under 60 fields of Up, and a 16-field Cross raised z 18032 to 18463. Captures `scratch/play/spyro3r/{settled,moved}.ppm` were opened: Spyro in the Sunrise Spring valley with the HUD. Blockers fixed at their causes: every stock CdRead completed instantly so the anti-tamper scan (cursor `[0x8006C6B8]` to `0x8007B6E4`, advanced only while the drive is busy) never finished (issue 0164); a CD completion arriving with a VBlank was never delivered because a VBlank-only guest element counted as claiming it (0164); the card HLE had B0:0x4E/0x4F swapped (0165). The boot prefix now returns after 887 fields in 669 steps and the title declares its own 1024-step bound. Still `partial`: no oracle comparison and no frame-time/audio evidence. The framework fixes for 0164/0165 are local and unpushed, so a CI checkout of psxport `main` still stalls at loader state 7. **Attract-demo freeze fixed 2026-10-02 (psxport `9da7eb6f`):** the CD controller raised masked responses, so libcd's handler consumed the disc check's polled GetTN and the music Setmode waited out CdSync's timeout; the interrupt now follows the enable register, results survive acknowledge, GetTN/GetTD answer from the disc TOC; 3/3 headless runs of ~8,400 fields pass both demo cycles with 0 CD timeouts, "DEMO MODE" capture shows Spyro moving | S008, S023 | G001, G002 **MEASURED 2026-10-01 (issues 0159, 0160, 0162): the title screen is PRESENTED.** Insomniac logo at step 300 (128,277 of 691,200 non-black), sign/egg/ninja at 600-900 (595,485-601,554), "Spyro Year of the Dragon" with "PRESS START" at 1200-1800 (621,849-622,011), then the attract demo "Entering Demo Mode" at 2400 (509,760). 0 `intr timeout`, 0 faults in 4500 fields after the native CdInit was removed so retail `CD_init` registers libcd's interrupt handler (0159). One "CD timeout ... CdlReset" from boot-time `CD_init` remains: the controller emulation does not answer the init reset. |
| S026 | Spyro 2 widescreen renders additional horizontal scene coverage without stretching: margin objects drawn from object memory past every title-owned horizontal cull, margin-only objects animated port-side, guest memory untouched | partial — psxport widens the guest-widened clip on BOTH ends (guestClipLeft/guestClipRight) and the full-display FillRect backdrop clear, so 16:9 shows 684x240 with the 4:3 picture centred and real scene in BOTH margins (right 90.0 %/143 colours, was 49.8 %/142 black; left 248 colours, was a 2-colour stale band). 4:3 byte-identical. `FUN_80043858` (the moby visibility walk) is native (`game/render/field/guest_moby_*`, 4074/4074 calls byte-identical to retail at 4:3) and widens its horizontal plane tests to the plan's projection width. The right 512..683 columns that were black on ~half of presented fields are fixed in psxport (the present-time margin base now covers only rows the guest did not draw in the displayed buffer; 24/24 consecutive 16:9 pshots full width). The terrain drawer `FUN_80023BB4` is native in full (`game/render/terrain/guest_terrain_*`, every pass, no guest hand-off; override differential 0 mismatches at 4:3 over the Glimmer routes and 60000 attract-demo fields, 12886 calls) with its per-vertex X outcodes on [-margin, 512+margin) at 16:9: the ground staircase at both edges is gone and terrain fills both margins in presented 16:9 Glimmer captures. `FUN_80046fd8`'s OFX/OFY 256/120 restore at `LAB_80047ea8` changes nothing visible (patched live to 342 and to 0: frames unchanged). Remaining: margin-only objects animated port-side is unmeasured, and the terrain re-split pass (`80028504`) is not shown to have run on the measured routes. On the Record path (2026-10-08) the guest's centre and draw rectangle stay retail's and the canvas holds the margins: 684x240 with both margins filled. The walked position still differs by aspect (issue 0178, cause measured 2026-10-10: the widened moby walk's class byte and the widened terrain packets reach the guest's own drawers, so guest time depends on the aspect; proper fix is a margin-only side list beside retail's, not done). | S024 | G003 |
| S031 | Spyro 2 widescreen anchors the UI: edge HUD elements at the widened edges or safe area, centred elements centred, nothing stretches | verified — the drawer is `FUN_80053E78` emitting 2D quads/sprites through `FUN_800520CC` (gem counter `FUN_8005251C` x 0x28 left, orb counter x 0x198 right, meter `FUN_80052D84` right, lives `FUN_80052B88` centre); guest 2D is presented centred (+margin), so `titles/spyro2/render/spyro2_hud_anchor.*` applies `ui_anchor::correction` (-86/0/+86 at 684) to the producers' x argument. Glimmer with Select (HUD on): 16:9 gem/digit at the left inset and orb/digit at the right inset matching 4:3, lives centred, unstretched, unclipped; 4:3 unchanged; override differential at 4:3 0 mismatches (counter 59/59, emitter 108/108). Gap: the meter path was not shown on the measured route | S026 | G003 |
| S027 | Spyro 3 widescreen renders additional horizontal scene coverage without stretching: margin objects drawn from object memory past every title-owned horizontal cull, margin-only objects animated port-side, guest memory untouched | partial — the widening itself is measured and rendered: the shared owner latches Spyro 3's OWN measured sites (`SetGeomOffset` leaf `0x8005D35C`; sole CR26 publication `0x8005955C` writes the projection distance H and is deliberately not intercepted; four inline CR24/CR25 restatements at `0x80033EF8/0x80033EFC/0x80034B30/0x80034B38` carried by the per-field re-assertion), authored window 512x240 OFX 256 OFY 120 H `0x155`, and presents 684x240 at 16:9 with 86px of real scene in both margins and the player unchanged in size. Both horizontal culls are native and widen: moby walk `0x80030478` (811 insns) and terrain drawer `0x80022378` (5487 insns) with its sector-visibility leaf `0x8002D0D8`, outcodes on [-margin, 512+margin). Override differential at 4:3 over `tools/title_route.py --title spyro3`: 26/26 moby-walk calls and 26/26 terrain-drawer calls match retail, 0 mismatches, 0 incomparable; the projection leaf's single sampled call is INCOMPARABLE ("original path performed platform-service @0x8005D35C"). 4:3 unchanged. Remaining: margin-only objects animated port-side is unmeasured, and the terrain re-split pass is not shown to have run. | S025 | G003 |
| S032 | Spyro 3 widescreen anchors the UI: edge HUD elements at the widened edges or safe area, centred elements centred, nothing stretched | partial — the HUD chain is measured and the edge widgets are anchored. Pass `0x80029E48` (from `0x8001E460`, bit `0x20`) dispatches one drawer per widget through the table `0x8006727C`; `0x800285A4` registers three groups whose DRAW callbacks are `0x80029904` and `0x80029BB0`, each emitting an icon through `0x800289C8(icon,x)` and a value through `0x800291B8(value,x,y,w)`. Measured elements and classes: collectable counter `0x80067248` (authored x 20) LeftEdge, lives counter `0x8006729C` (authored x 256) Centred, egg counter `0x800672F0` (authored x 492) RightEdge. Classification is by widget element, not by return address, because both counters share one drawer and reach the SAME emitter call sites (icon ra `0x800299B8` for gem x 20 AND egg x 382), so `spyro::hud_draw_context::Draw` carries the element down one level. The icon emitter is NOT corrected on every call: the value emitter `0x800291B8` draws its digits by CALLING the icon emitter `0x800289C8` per glyph from `0x80029374`, so correcting both applies the margin twice — measured as the collectable count landing at x −111 (off the left edge, number not drawn) and the egg icon at 595 instead of 468 (jammed to the right edge, reading as icon/count swapped); the value emitter therefore opens `hud_draw_context::Draw::insideValue` and the icon emitter leaves that interval alone. Presented evidence at 684x240: collectable icon at columns 21..49 (4:3: 20..50) and its digit at 61..84 (4:3: 61), egg digit at 595..618 (4:3: 423..446, i.e. +172 = the full widening), icon left of count on both sides, nothing resized. 4:3 is PIXEL-IDENTICAL: 0 of 122880 pixels differ from the pre-change reference. Gap: widgets that only appear under other game states have not been enumerated. | S027 | G003 |
| S028 | Spyro 2 presents interpolated 60fps | partial — on the Record path (`RenderPath::Record`, temporal interpolation on), measured 2026-10-08 on the Glimmer route, guest timing unchanged (gameplay at field 3950, fade loops 32 fields). Real field: `PSXPORT_DEBUG=recordcheck` mismatched=0 at 1x 4:3 with fps60 off (2668/2668) and on (2678/2678), and at 16:9. Producer: the terrain drawer 0x80023BB4 saves its field's camera and sector visibility, and `TerrainStateProducer` redraws the terrain from the drawer at every t; every packet carries one object and one of `spyro::depth_bins`' bins. Cuts: game state or level change, sampled when the draw that walked the table returns (`spyro::FrameCut`). Interpolating: gameplay records alternate walked/empty and the presenter and record rasterizer pair the two shown walks across the empty one (psxport 11376e21, b74684e4); every gameplay in-between is drawn (730 drawn, 10 refused, all before gameplay). Walking preseq: 3.97 3.50 0.00 4.23 3.70 0.00 4.53 3.86 0.00 6.51 4.77 0.00 (in-between, real, repeat per walk). 1x 4:3 with fps60 on through the title, intro cutscene and a walk (2026-10-08, `scratch/record/v7/orderfix/run.log`): 2047 plain and 610 composed recordcheck lines, all mismatched=0, and the walking in-betweens show no slivers. Cuts also cover a camera re-placed inside one state (2026-10-10, issue 0174): `spyro::FrameCut` declares the camera controller's three placement steps from its own mode machine (mode 9, 0xB, 10; `0x80067ED0`/`0x80067ED4`); test `a_cut_is_a_camera_placement_inside_one_scene`, and with the game's own request word set to 9 the log shows `cut=true camera=9/1` and the 136 presents show no frame holding both places. Recordcheck at 1x 4:3 (`aspect=0`, `ires=1`) on the Glimmer route after this change: fps60 off 2627 of 2627 lines mismatched=0, fps60 on 2633 of 2633. Remaining: mobys, HUD, sky and particles unkeyed (0176). **Real time MEASURED 2026-10-13 (issue 0183), player build, mapped window, load ~1-3:** guest VBlank 60.0/s, 30 real + 30 in-between presents/s, audio 44,100 samples/s with 0 drops after the boot prefix returns (was 88,000 samples/s, 120 SPU fields/s, ~120 drops per 2 s); the boot prefix still delivers bursts of up to 120 fields per present and drops ~100 audio fields. | S024 | G003 |
| S029 | Spyro 3 presents interpolated 60fps | partial — on the Record path since 2026-10-15, as S028 on the same shared producer: the terrain drawer 0x80022378 saves its field (camera, sector visibility, packet budget, every guest word it read) and `TerrainStateProducer` redraws the terrain at any t; the flatten 0x8004E664 bins every packet (the second-pass flattens of states 4 and 0xD assign none); `spyro::FrameCut` reads game state 0x8006E344, level 0x8006C5BC and the camera machine 0x8006E044/0x8006E048. `PSXPORT_DEBUG=recordcheck` at 1x 4:3 with fps60 off over 3000 frames from boot: 3001 of 3001 lines mismatched=0, 1464 of them composed at t=1 (Spyro 2 the same run: 3001/3001, 2034 composed). Tests: `tests/test_spyro3_record.cpp`. Remaining: real-time and in-between picture checks on this path are not recorded; mobys, HUD, sky and particles have no producer. | S025 | G003 |
| S036 | The zero-argument product opens on an in-window title selector; entries are enabled only for provisioned, authenticated executables; a title can return to the selector and another can start in the same process | verified headless, both orders, for all three pairs (1<->2, 2<->3, 1<->3) at frame 300, partial overall — selector rendered by the psxport RmlUi screen and captured from the present image; runs selector -> A (frame N) -> `session return` -> selector -> B (frame N) and compares B's full guest RAM and scratchpad with two fresh-process B runs (the two fresh runs agree first, as the control): identical in both orders (the control also differs between titles: 36f2a838 / 3ab8187f / 74f52d7f). Defects found and fixed on the way are in issue 0169. a pad-driven (not control-channel) selection of every title, and any windowed run, are not recorded; the picker window has no title (the pinned framework has no HostIdentity); the explicit executable argument remains a maintainer override that skips the selector | Panel logos: a title's wordmark is snapshotted from that title's OWN live VRAM while the game's own title screen is on it — the facts name the title's gate (the guest word whose overlay block tick retail itself tests) and its wordmark's sprite records, and `extractPanelLogo` (game/core/title_logo.cpp) reads those records and snapshots their texture rectangles through their CLUTs, resolving index 0 as the PSX transparency rule says. **Spyro 1 verified**: sprite record 0 at 0x8006FACC (tpage 0x0098 = 8-bit, clut 0x7FE0, 255x128) emitted by the title overlay's 0x8007CEE4, gated on CutsceneLayout.m_CurrentTick >= 1170 via the pointer global 0x80075680; the CLUT is 256 texels in ONE VRAM row (beetle DEFINE_Update_CLUT_Cache), not a 16x16 block — the block read is what produced per-texel noise inside a correct silhouette; the rectangle's right 127 columns are the banner's texture (its page origin is 128 texels right), so the fact clips to 128 and the panel shows the SPYRO THE DRAGON wordmark (`scratch/picker_verify/sel_spyro{1,2,3}.png`, 2026-10-03, three lit panels, wordmark centred at 20% height in colour on the selected panel; Spyro 2/3 logos still `blocked`, their tables ungated, so those panels draw no name). LOGO SCALE 2026-10-04 (the 2026-10-03 note claiming 3x was WRONG): measured on the shipped path, `pictureMagnification()` (external/psxport/runtime/psx/host/picker_composite.cpp) computes `cover.h / (crop.h * pictureHeight)` where `pictureHeight` is the HELD image's picture height, not the guest's 240 lines. A panel session's pane image is 480x720 with the 16:9 picture letterboxed to `viewport 480x269`, so the factor is 720/269 = 2.677, which is not whole, and the function returns 1: `logo panel 0 image 128x128 magnified 1x` in every run, while the picture beside it is magnified 2.677x by the pane compositor's linear sampler. The logo is therefore drawn at 1/2.7 of the picture's scale. THREE LIVE PANELS, MEASURED 2026-10-04 (`scratch/panels/`, build 4a10062, `tools/shipping_settings.ini`, fps60 off because the live psxport painter plan aborts in Spyro 1's demo at issue 0172): the brief's hypothesis is REFUTED — a paused panel DOES still present into its pane, because `PaneCompositor::composite` samples `lastFilledPresented()` (the per-session hold, `gpu_vk_internal.h:192`), not `lastPresented()`, and `panelReady()` probes a paused panel that holds nothing; headless and windowed composites both show three lit panels, the two unselected ones greyed (`scratch/panels/headless_fps60-0_shot2.png`, `window_fps60-0_shot2.png`, all three 100.0% non-black, 100%/100%/179 distinct colours). The black panels are the BOOT phase, and the cause is that `PanelSessions::advance` runs exactly ONE guest at a time, so the three boots are serialized: in the player's leg (windowed, paced, offscreen swapchain, `scratch/panels/off/`) panel 0 first holds a picture at 7.9 s, panel 1 at 12.3 s, panel 2 at 20.1 s, and all three are `demo` at 84 s — 67 picker frames (1.0% of a 268 s run) have exactly one lit panel, which is the reported screenshot. Panel frame rate: 27.7 fps / 36.1 ms windowed-paced over 5103 settled frames, 100-111 fps / 9.0-10.0 ms unpaced (headless and windowed agree), so the picker paces its single panel step to a display field in addition to its own field. The picker also owns no `QuitScope`, so an `SDL_EVENT_QUIT` in the selector loop exits the process at `host_input.cpp:34` (measured: exit 0 after 5-15 s under Xvfb). Spyro 2 and 3 remain blocked and the measurements that block them are recorded in their own logo-facts headers: Spyro 2 has no sprite table in 2 MiB of RAM, its display list draws no textured prim, guest VRAM is never written, and the on-screen plate's texels match nothing in the saved machine; its 0x8019C000 buffer holds a different, vertically sliced lockup. Spyro 2/3 logos: the MDEC/STR route is REFUTED for both, not assumed — no `[fmv] begin` in any run that reaches either title screen, no `[cd]` streaming after boot, and no `.STR` directory entry in either disc image. Two earlier Spyro 2 negatives were wrong and are corrected in its logo-facts header: guest texture memory is populated, not empty, and `provat`'s `<never written>` proves nothing (it says the same for Spyro 1, whose logo texels are resident). The open lead is an INDEXED texture page in that populated memory; the plate's 15-bit form matches nothing in RAM. SPYRO 2/3 LOGOS ARE BLOCKED, and parked there deliberately. Spyro 2's overlay bundle went through Ghidra at its measured base (WAD[0x237dc..0x35800] -> 0x8006DA40) and does not contain the emitter: the 19 module bases are pointer-setup functions with zero `jal` in 8 KiB, SCUS_944.25's own file has no record-shaped table, and `refs 0x80066D40` returns nothing, so the title state's draw cannot be reached from the one word known to select it. Every measurement is in the titles' logo-facts headers. What is landable today is Spyro 1's logo, the shared magnification, and the selector layout.
| S033 | Spyro 1: load operations complete without loading-only waits or presentation; logos cancel through the recovered route | partial — M1 and M2 measured against the full-console reference (28/28 payload digests identical; 0 hand-off field differences at 4 terminals) and R6 decided; M4 (absence of loading presentation) still open | S008, S011 | G005 |
| S034 | Spyro 2: load operations complete without loading-only waits or presentation; logos cancel through the recovered route | missing | S008, S024 | G005 |
| S035 | Spyro 3: load operations complete without loading-only waits or presentation; logos cancel through the recovered route | missing | S008, S025 | G005 |

## Current focus

### Spyro 1, 2 and 3 on the Record path

All three titles present on the Record path, with 60 fps as psxport F-FPS60 (`docs/features.md`).
Spyro 2 and Spyro 3 interpolate their terrain through the shared
`guest_terrain::TerrainStateProducer` (S028, S029); a saved field carries every guest word the
drawer read, as first read, because the guest rewrites level words between a field's draw and its
present. Spyro 1 composes its screen overlays, terrain, sprite queue, Spyro, sky, drop shadow, glows
and particles (S020). Open: Spyro 1's portals and remaining effects (issue 0186); Spyro 2/3 mobys, HUD, sky and particles (issue
0176); the aspect-dependent walk (issue 0178, proper fix is a margin-only side list).

## Hosted verification and host gaps

Hosted CI is asset-free: one Linux Clang job runs `tools/verify.py` (build, CTest, clang-format,
clang-tidy, ownership policy). It downloads no disc, executable or BIOS and claims no gameplay
evidence. macOS arm64, Windows x86_64 and Android arm64 runtime jobs are missing until platform
packaging provides real target builds and execution evidence.

## Capability details

### S001 — Spyro 1 boot to title

Evidence: the identity-verified `SCUS_942.28` shipping runtime reaches stage 13 and the resident
OV_5B800 title overlay on both reference and native render legs at framework pin `99a42aa3`. The
serialized reference record `scratch/logs/title-menu-reference-99a42aa3.log` contains 35
`[titleoracle] PASS` results, no `DIVERGES`, `REFUSED`, `STUCK`, or `FATAL`, and a clean
3,000-present cap. Supplemental native captures `present_50.ppm`, `present_150.ppm`, and
`present_250.ppm` show a coherent Universal logo, Insomniac mountain scene, and Spyro title scene,
with measured non-black coverage of 4.08%, 93.26%, and 93.33%. Issue 0085's retained exact command,
denominators, and result are the durable record; the gitignored run artifacts are supporting evidence,
not the sole verification basis.

Gap: the historical retained-renderer comparison above has not been repeated against the current
independent full-console oracle. Current native/Lightrec title and gameplay observations are recorded
in S009 and S011; complete boot/state parity remains unqualified.

### S002 — Title modes 0 and 1

On Record the guest's title overlay draws modes 0 and 1 itself; recordcheck at t=1 on the boot
route through the title screen to the attract demo reports `mismatched=0` (S005). The native title
owner (`0x8007CD38` sprite emitter recipes) was deleted on 2026-10-15.

Gap: no title-screen producer, so its in-betweens repeat the shown record; widescreen does not move
its elements (issue 0186).

### S003 — Title mode 2

On Record the guest draws the three-slot save screen. The 900-field forced-input mode-2 route was
verified on the deleted native owner and has not been rerun on Record. `[0x80078D7C] == 2` means a
paired actor only in title mode 3 and also names save-picker state 2.

Gap: rerun the mode-2 route on Record with recordcheck.

### S004 — Native frame and field ownership

Evidence: `Spyro1Runtime` creates one title-local `Spyro1FrameDriver`; `psx::host::TitleSession::boot`
returns and the framework shell calls one finite `stepFrame`. The driver owns the measured input-latch/update/frame-
step/render order, while `FieldScheduler` owns the 60 Hz counter, pad service, guest callback root,
audio, BIOS events, present/pace, host-turn acknowledgement, and producer boundary. Boot
`0x800127C0`/`0x8001286C` is recovered as a resumable sequence of its four eight-field fades, two
210-field holds, CD/state pump, and final display initialization. The adapter config declares
libetc VSync `0x8005DBC4` as the mandatory fatal trap; helper `0x8005DD0C` has no success override.
The retired product kept emitted boot bodies as A/B references. The native/Lightrec product must use
an independent emulator or separately built test oracle instead.

The real `SCUS_942.28` product path now supplies the runtime proof. Clang build
`scratch/build/agent-spyro` passed 34/34 CTests, then
`scratch/logs/agent-spyro-wide-capture-600.stdout.log` exited 0 at an 800-field cap after the 436-field
boot and 182 native logic frames. The framework reported its frame-loop contract satisfied, all
three stage-13 native producer rows were exercised, and neither the guest-VSync trap nor the
one-presentation-fence assertion fired. Issue 0087 and C225 hold the exact causal sequence and
falsifier. The retained reference renderer `0x8001ED5C` still contains guest VSync and deliberately
fails fast until its diagnostic display tail is split; that separate diagnostic gap does not regain
product timing ownership.

The forced-input mode-2 path separately reached a guest render-suppressed update. The old cadence
predicate accepted that iteration without a field, contradicting the framework's exact one-fence
contract. The scheduler now delivers one visible host field with the previous picture; the ensuing
900-field native/wide/interpolated run reached 415 logic frames and satisfied the same contract.

Gap: independent oracle timing/device comparison and representative interactive gameplay cadence
remain unqualified beyond these bounded native frame observations.

### S005 — Spyro 1 presentation on the Record path

`Spyro1Runtime` presents on the Record path (`docs/features.md`, F-RENDER and F-FPS60):
`Spyro1FrameDriver` runs the retail update, `spyro1::FrameDraw` runs GamestateDraw `0x8001ED5C`
to its return with each `VSync(0)` of its tail delivered as a field, and the guest's own
PutDispEnv/PutDrawEnv/DrawOTag draw the frame on the device. `guestPictureIsNativeWidth` is true
until the boot logos are done.

Evidence: `PSXPORT_DEBUG=recordcheck` at 1x 4:3 over 3000 steps to the attract demo's gameplay: 2566
of 2566 checked records `mismatched=0`; the 24bpp boot logos are not record presents. Tests:
`tests/test_spyro1_record.cpp`, `tests/test_spyro1_screen_overlays.cpp`.

Gap: the portal bodies and the effects listed in issue 0186 have no producer, so they repeat the
shown record in an in-between. The guest facts recovered by
the deleted native renderer are in `docs/findings/spyro1-field-draw-families.md`.

### S006 — Spyro 2 measured boot boundary

Binary analysis of the exact manifest-matched `SCUS_944.25` executable identified 683 resident
function entries with no foreign-title or overlay assumptions. The retired dedicated `spyro2_port`
kept its emitted guest bodies separate from Spyro 1, proving the images cannot share address-derived
dispatch. `Spyro2Runtime` has no Spyro 1 compatibility state or hooks. The retired title-owned finite boot driver reproduced
the binary's persistent game-main and boot-prefix stack frames, calls constructors `0x80054834`
and first leaf `0x800548A4`, then enters a title-owned display-bootstrap state machine instead of
dispatching either the non-returning guest main or the retained bootstrap. That owner preserves the
exact nested stack/register state and non-timing effects around three measured field waits: two
direct calls from `0x80011BBC` and a third inside clear helper `0x8004C484`. DrawSync `0x800557E4`
and GPU timeout arm/check `0x80057880`/`0x800578B4` are title-owned synchronous overrides; none
spends a display field.
The Clang product and focused runtime, launcher, structure, provisioning, and executable-help gates
pass. An isolated real-executable run passed shared boot initialization, audited crt0 10/10, and
reached the title-owned `0x80011BBC` boundary with fatal VSync registration intact.

Live PID `3564943` presented all three finite fields, each captured and visually verified as the
expected uniform black clear picture. It completed display bootstrap, selected NTSC 59.940 Hz,
returned to `0x80011EB4`, and deliberately stopped at later binary-owned boot-prefix leaf
`0x80011B1C`; no guest VSync violation occurred.

Gap: issue 0092 owned the post-display initialization and loader chain. The loader's module at
`0x80077374` is outside resident text; its source (LBA 570, 37 sectors), destination (`0x8006D264`) and
identity (SHA-256 of the landed bytes) are established and it is published as an image (issue 0156), so
the boot prefix returns and the retail per-frame loop runs into gameplay on the Record path;
widescreen and 60 fps are S026 and S028.

### S007 — Spyro 1 gameplay input

The retail movement routine already consumes held digital input. The authenticated `SCUS_942.28`
loads `[activePad + 4]` at `0x8003D4A0` and `0x8003D4BC`; this is the buffered held word.
The reference decomp declares `g_ActivePad` as the outer `Gamepad*`, where the same `+4` offset is
named `m_Released`. Treating that mistaken type name as buffered release-edge semantics introduced
a redundant native movement override. The guest selector at `0x80043FE4` instead selects the
24-byte buffered record at `0x800773BC + 24 * substep`. The override and its shift-only test are
removed; movement remains owned by the unchanged retail body through Lightrec.

`test_spyro_digital_input` is a local, asset-dependent production-boundary discriminator: it
authenticates the supplied executable, runs press/hold/release/idle states through the retail
function, and checks target speed/rotation, turn momentum, preserved ABI and nonzero JIT execution
with zero fallback. Run explicitly after building the target:
`build/test_spyro_digital_input <path/to/SCUS_942.28>`. It is not an asset-dependent hosted
CTest gate. The operator's focused run passed all 4/4 cases: press and hold each executed 3 JIT
blocks / 40 instructions and produced speed 1536, rotation 992; release and idle each executed
3 blocks / 33 instructions and produced speed 0, rotation 592. Total execution was 12 JIT blocks
/ 146 instructions with zero interpreter fallback. This proves the isolated retail input-consumer
contract and preserved ABI, not representative gameplay conformance.

Historical idle-versus-Left replays changed player position after the New Game handoff, and jump,
charge, and flame reached the guest update. Those observations do not establish the false
release-edge explanation or complete current gameplay parity.

Gap: current interactive jump, charge, flame, and controller parity still need representative
gameplay comparison against the independent console oracle.

Live host input now reaches the pad: `Pad` takes its key state from KEY_DOWN/KEY_UP events instead
of SDL's keyboard-focus-gated state array, and every drain of the host queue feeds it (psxport
`d7a9fdfd`), so a real press ends Spyro 1's logo holds and reaches the guest pad packet.

### S008 — Runtime Lightrec execution

Implemented subset: the product enters authenticated crt0 through psxport's per-`Core`
`dispatchGuest` boundary. Image-aware native dispatch, scoped `callOriginal`, invalidation, and typed
exit contracts are present, the frozen PSXport/Lightrec backend is linked, and the synthetic framework
contract passes. The old generator/dispatcher/selector/product are absent.

Real-media evidence (2026-09-05, Linux x86_64, Clang, framework `eb5f23a8`): the authenticated
`SCUS_942.28` initially stopped at `0x80064F0C` after one cycle budget, with 43,550 executed JIT
blocks and zero fallback. The root entry point incorrectly treated a normal budget yield as fatal.
`GuestExecution::step` now preserves the initial return address and committed continuation across
budgets; it propagates every other exit without silently resuming it. Four synthetic production-path
scenarios cover an uninterrupted nested call, the same call over 125 yields, an unknown-image fault,
and a native frame exit. Both nested-call variants execute 1,002 JIT blocks / 4,008 instructions and
produce the same result with zero fallback; repeated terminal steps execute nothing.

A debugger observation at the next entry after 100 completed executor calls measured
844 translations, 3,090,158 executed blocks, 21,759,844 executed instructions, 258 host dispatches,
3,089,314 cache hits / 844 misses, zero executor faults, and zero fallback blocks/instructions.
The continuation was `0x8001647C`. The real disc opened; the 15-second headless/no-audio observation
reported CD synchronization timeouts and was externally bounded. This establishes continuation and
real-image JIT execution, not successful boot or CD correctness.

Current integration (2026-09-08): Spyro now composes its native boot, display-field, and scene
owners around bounded guest calls. The framework exposes stock CD command success separately from
blocking-control success and supplies the title disc key. The title WAD owner stages complete reads
before publication, refuses truncated input with a typed fault, derives image identity from SHA-256,
and relies on canonical guest writes for invalidation. Synthetic production tests cover short reads,
per-Core completion, changed-code execution, and missing-disc faults. These changes pass the recorded
stage-13 routes below; the prior CD timeout no longer describes the current frontier.

Gap: representative gameplay, independent-oracle state, and real-game WAD replacement/override
coverage remain unqualified. Synthetic changed-image tests do not prove every gameplay overlay.

Atomic work: issue 0101.

### S009 — Stage-13 dynamic discriminators

Evidence: the Linux x86_64 Clang product, using framework `f7d4baf5`, completes the 800-field idle title
route and the 900-field Start-input save-picker route through native field/scene owners. Guest VSync
remains the framework's mandatory fatal trap. Run caps now return from the complete product step
before ending, so the final presentation fence is checked and the Core is destroyed normally.

The idle run ended at 800 fields / 600 checked steps and fences, with 3,144,960 executed JIT
blocks / 20,096,897 instructions and zero faults/fallback.

The 16:9 save-picker observation ended at 900 delivered fields, 474 completed product steps and 474
presentation fences, with 1,485 translated blocks, 3,116,634 executed blocks / 20,245,455 instructions,
zero executor faults, and zero fallback blocks/instructions. It reached stage 13/mode 2/state 4.
The inspected presented image contains all three slots, menu/footer text, Spyro, and expanded
backdrop; native render width is 684 versus the 512-wide baseline. Interpolation was enabled;
this still image and menu run do not qualify smooth gameplay or all renderer arms.

Reproduce using `build/bin/spyro_port` with `PSXPORT_NOAUDIO=1`, `PSXPORT_VK_WINDOW=0`,
`PSXPORT_ASSET_DIR=external/psxport`, `PSXPORT_NATIVE_FRAMES=474` (fence steps; the 900-field run above), and
`PSXPORT_FORCE_BUTTONS=FFF7`. Use `PSXPORT_SETTINGS` pointing to an isolated file containing
`aspect=1`, `ires=1`, and `fps60=1`; `PSXPORT_ASPECT` is not a supported option.
`PSXPORT_PRESENT_SHOT_AT=450` captures the inspected menu. The idle discriminator uses
`PSXPORT_NATIVE_FRAMES=600` (fence steps; the 800-field run above) and no forced input. These observations establish wiring, not representative gameplay.

### S010 — Resumable world execution

Implemented subset: `game/core/world_body.inc`, its native transcription, and emitted-body calls are
gone. `WorldGuestExecution` resumes unchanged retail `RenderWorldChunks` at `0x800258F0` through the
scoped-original runtime boundary.

Gap: the Lightrec backend must execute that boundary and prove bounded host-service exits resume the
same guest CPU state. No body transcription or interpreter fallback may replace it.

### S011 — Representative gameplay conformance

Evidence: Spyro 1 reaches Artisans gameplay through native/Lightrec execution, accepts held input and
moves Spyro, with zero faults and no interpreter fallback; Spyro 2/3 reach moving, jumpable gameplay.
`VSync(-1)` reads `0x800749E0` (`psyq.s:2646-2649`). Guest facts recovered on the deleted native
renderer are in `docs/findings/spyro1-field-draw-families.md`.

Missing capability: a bounded interactive Spyro 1 route must reach at least the current gameplay
frontier with native and scoped-original dispatch, positive and controlled-negative WAD invalidation,
independent-oracle timing/memory/interrupt/device comparison, and the declared correctness/frame-time
budget on every released host architecture.

### S012 — Native/Lightrec launcher

Implemented subset: the frozen launcher authenticates the user's disc, provisions only the selected
PS-X EXE, builds the sole `spyro_port` native/Lightrec target under `build/player`, and launches Spyro
1 by default without offline guest translation or a pre-populated runtime cache. The native target
links the frozen PSXport/Lightrec backend, and its asset-free synthetic framework contract passes.

Gap: cold-path provisioning and packaged first-run setup remain unqualified. The native boot/title
routes are recorded in S009; representative gameplay and host qualification remain open.

### S013 — Windows release

Missing capability: a Windows build/package CI job, installable asset-free release, first-run file
selection, and synthetic execution/package checks on the supported Windows architecture. The
current hosted workflow performs Linux source-policy checks only. Real-title gameplay and
performance remain local qualification requirements.

### S014 — macOS application release

Missing capability: a macOS `.app` bundle and release CI, matching-architecture dynarec/ABI/cache
checks, first-run file selection, persistent user data, and local gameplay qualification. Apple
Silicon support requires an actual AArch64 backend and executable-memory/instruction-cache
qualification; interpreter fallback does not satisfy it.

### S015 — Linux AppImage release

Missing capability: an AppImage builder/release job and cold package-launch/install tests. The local
Clang executable and interactive observations are useful inputs, but do not qualify a relocatable
AppImage, its runtime dependencies, or no-terminal first-run selection.

### S016 — Android APK release

Missing capability: APK packaging/release CI, ARM64 runtime qualification, game-file selection,
authored multi-touch controls, and measured performance on named device classes. Build and emulator
mechanics belong in `shared/android-port`; Activity/SAF/lifecycle mechanics belong in Lucent. The
title owns identity, complete-install policy, touch meaning/layout, and composition. Desktop
measurements and compile-only CI do not establish Android gameplay support.

### S017 — WASM release and GitHub Pages deployment

Missing capability: a browser build using the same runtime guest-discovery/execution contract,
browser package tests, and a CI deployment to GitHub Pages. Browser dynamic execution must preserve
JIT-first semantics with bounded accounted fallback; a missing web backend cannot silently select
an interpreter product. Acceptance includes user-selected local game files, persistent browser
saves, controllable gameplay, audio/input correctness, and measured browser performance. No game
files may be embedded in the deployment.

### S018 — Packaged game-file setup

Missing capability: desktop/mobile native file pickers and browser file selection that authenticate
the complete supplied install, persist it in the OS/browser user-data location, support reselection,
and preserve a prior valid selection on every failure. The existing launcher authenticates and
provisions the disc through maintainer inputs; it does not provide packaged first-run UI. Shared
archive/SAF mechanisms must be consumed from their canonical owners rather than copied into Spyro.

### S019 — Additional widescreen scene coverage

Spyro 1 widens its canvas through `SpyroRuntime::guestWidescreenProjection`, with its projection leaves
`0x80062618`/`0x80062638` (684x240 at 16:9); the guest's centre and draw rectangle stay retail's.

The terrain's sector plane and face clip draw the widened window (native RenderWorldChunks), and
so do the moby list planes (BuildActorDrawList `0x8001F158`, `0x800208FC`) and the sprite queue
`0x80022A2C`, each a byte-exact native port.

The sky `0x8004EBA8`, glows `0x800580F4`/`0x800584C4`, particles `0x800573C8` and the moby
emitters `0x8001F798`/`0x80020F34` test x on the widened window too; a moby straddling the 16:9
edge keeps its polygons between the 4:3 and 16:9 edges (`tests/test_spyro1_actors.cpp`). Widening must not write guest state: the particle seen byte and the sparkle
fade are written from retail's 4:3 window (`spyro1::kRetailBounds`), whatever the widened window
drew (`tests/test_spyro1_effects.cpp`); a moby's drawn byte +0x51 and the sector marks
`0x800771C8` and sector animation follow retail's plane and sphere, since level code reads them
(`tests/test_spyro1_actors.cpp`, `tests/test_spyro1_terrain.cpp`).

Gap: the portal bodies of `0x80050BD0` clip at 512 (issue 0186).

### S020 — Spyro 1 60 fps

`Spyro1Runtime::interpolates()` is true; in-betweens are psxport F-FPS60. Producers: the fullscreen
tint `0x800190D4` and the cinematic letterbox `0x80018F30` (`spyro1::ScreenOverlays`), each keyed by
its call site and slotted by the LinkOTPrimitives override `spyro1::OrderingTable`; the terrain,
RenderWorldChunks `0x800258F0` (`spyro1::terrain::TerrainImage`, `tests/test_spyro1_terrain.cpp`);
each record of the sprite queue `0x80022A2C`, Spyro `0x80023AC4` and each moby the emitters
`0x8001F798`/`0x80020F34` draw (`SpriteQueueProducer`, `PairedActorProducer`,
`ActorEmitterProducer`, `tests/test_spyro1_actors.cpp`), position and angles moved at t; the sky
`0x8004EBA8` (rotations at t), the drop shadow `0x80059A48` (ground point and heading), each glow
emitter `0x800580F4` and sparkle `0x800584C4`, and each particle record `0x800573C8` (points and
angle when the kind is unchanged) (`tests/test_spyro1_effects.cpp`). recordcheck
1x 4:3, 3000 steps: 2566 of 2566 records `mismatched=0`, 2559 composed at t=1. Tests:
`tests/test_spyro1_screen_overlays.cpp` (t=1 composes the drawn frame, t=0.5 the frame drawn
halfway). recordcheck composes the tint at t=1 with `mismatched=0`; the census shows keyed 44, all
under a producer with a render.

Gap: no producer for the portal bodies or the remaining effects (issue 0186); the census, attract
route, 6000 steps, leaves 35962 of 7823787 primitives unkeyed, all from those. No real-time check of this path is recorded.

### S021 — SVG touch-control interface

**Status: missing.** Touch-enabled releases require an authored SVG overlay, reachable multi-touch
layout, safe-area handling, input cancellation, and controller-aware hiding or configuration. Touch
controls must use the same title action policy as physical controllers. No integrated or qualified
Spyro touch overlay exists yet; Android runtime mechanics belong to Lucent and build/device mechanics
to shared/android-port.

Related goal: G004.

### S022 — Spyro 1 music (XA streaming)

**Status: partial.** Music now streams. Spyro's sound driver sets mode `0xC8` plus a file 1/channel 4
filter and issues `ReadS` **without** a `Setloc` — it scans forward and polls `GetlocL` — and the
shared XA streamer treated a foreign (other file/channel) EOF as the end of that open-ended stream,
which happened seventeen sectors in: `xa_sectors=0`, `xa_wr=0`, `xa_pulls=1` for 3,500 fields while
the sink received a slowly-varying constant on both channels. psxport `379eafea` skips a foreign EOF
instead (a bounded clip still ends at `end_lba`, and only our own channel's EOF ends an open-ended
stream). The same windowless 3,500-field capture now decodes 959 XA sectors with a real waveform
(zero crossings 900–2400/s, RMS 1750–4134). Still open: the sink carries a ≈+1430 DC bias even while
the SPU is disabled (the CD-audio source is ruled out — `CDC_GetCDAudioSample` writes zeros when
nothing is streaming — so it enters in the SPU mix or the sink conversion; it is inaudible on a
device but it is why "non-silent PCM" cannot be used as an audio test); the intro cutscene's music
has not been compared against the console's own PCM;
nothing has been verified through a real audio device (headless `PSXPORT_WAV` captures only).

**What that scan actually costs, measured 2026-09-19.** "Scans forward" is not a figure of speech and
it is not free. The port keeps a SECOND disc cursor: the data path tracks the head in `cd.sec_lba`
while the XA stream keeps its own `s_lba`, which starts at 0. On the `tools/drive.py gameplay` route
the stream started at LBA 0 and the scan reached LBA 53,874 — about 122 MB and 7,400 CHD hunk
decompressions — **inside a single 1/44100 s output sample**, because the pull loop re-enters
`xa_decode_next_sector` as soon as its 64-sector guard expires. That was the three-second stall
recorded as [issue 0115](issues/0115-the-first-gameplay-frame-stalls-three-seconds-in-cd-audio.md).

**Resolved 2026-09-19.** The open RE question — "no `Setloc` is logged before the read at all, so
where does the driver believe the head is?" — had a third answer neither hypothesis covered: libcd's
`CdControl` (`0x80063EAC`) consults a per-command table (`0x80074DAC`) and sends the Setloc ITSELF
for `0x03`, `ReadN`, `SeekL`, `SeekP` and `ReadS`. Spyro's sound driver at `0x800568D0` does
`CdIntToPos(lba, &loc); CdControl(CdlReadS, &loc, 0)` and needs no Setloc of its own. The framework
override that replaced `CdControl` read only the command byte, so the position was thrown away.
Fixed in psxport `892e9550`, which applies the position through one owner shared by the explicit and
implicit Setloc; the stream now starts at the traced LBA 113,448 and the scan is gone. The
second-cursor observation stands as a design note — pacing head advance at the guest's declared
drive speed is still worth doing — but it is no longer needed to end the stall.

Related goal: G002.

### S023-S029 — Spyro 2 and Spyro 3

Both titles run their retail boot prefix through Lightrec, reach gameplay from their own title
screens, widen to 16:9 and present on the Record path with interpolated terrain (rows S023-S029,
S031, S032). Their 60 fps is psxport F-FPS60 through the shared `TerrainStateProducer`
(`docs/features.md`); mobys, HUD, sky and particles have no producer (issue 0176).

| | identity | entry | game main | libetc VSync | measured boundary |
|---|---|---|---|---|---|
| Spyro 2 | `SCUS_944.25` | `0x8005478C` | `0x80011ADC` | `0x80058EDC` | boot prefix returns after 43 steps / 133 fields, then the retail update/draw run 784 steps with 0 faults (issue 0156, S006/S024) |
| Spyro 3 | `SCUS_944.67` | `0x80059444` | `0x8001200C` | `0x8005956C` | boot prefix `0x8002AB38` returns after 336 steps / 546 fields (issue 0156); the main-loop draw's fault at `0x8001C4EC` is fixed (issue 0158) and gameplay is reached (S025) |

**A note on the `libetc VSync` column, because it is the one fact that is easy to state wrongly.**
The function-reach report lists `0x8005956C` among the addresses reached, and that is **not**
evidence the guest VSync body ran: `lightrec_executor.cpp`'s `blockBoundary` calls
`reach->observe(guestPc)` *before* it asks `classifyGuestHostDispatch`, so the recorder sees every
address the executor examined, including the ones it then handed to a native owner and refused to
translate. A reach entry is evidence of **dispatch**, not execution.

The measurement that does settle it is a store observer armed on all six store instructions inside
the VSync body, plus `0x80050620` as a control that is provably on this boot path: **6 of 6 VSync
stores recorded zero executions across 135,585,657 JIT instructions, while the control executed.**
One run, one instrument, both classes. Issue 0153 §4 carries the full arming.

Related goals: G001, G002, G003.

### S030 — Spyro 1 widescreen UI anchoring

Gap: on Record the HUD is the guest's own and sits at its 4:3 columns. RasterizeSpritePrimQueue
`0x80022A2C` sets the GTE offset per HUD moby inline (`0x80022D2C`, `0x8002395C`), so there is no
leaf to override; anchoring needs the shaded-queue producer (issue 0186). The anchoring rule is
`spyro::ui_anchor`, as Spyro 2/3 use it; the HUD layout and classes are in
`docs/findings/spyro1-field-draw-families.md`.

### S033 — Spyro 1 loading removal

Missing, and now MEASURED rather than asserted. Censused statically in `docs/issues/0155`: 31 CD read
issuer sites (11 blocking, 20 streaming), no loading screen exists, and the port's CD overrides
already remove storage latency.

**M3 is done (2026-10-01, `docs/issues/0155` "M3 measured", instrument I058).** The `LoadLedger`
(a measurement tap fed by `cd_queue.cpp`; it was never
committed) records one entry per CD operation: issuer
site, guest PC, LBA, length, destination, the SHA-256 the image identity already computed, blocking
or streaming, `g_LoadStage` at issue and completion, the XA bit, and the field at issue and
completion. `tools/drive.py gameplay` to `GS_Playing`: **26 operations (7 blocking, 19 streaming),
0 pending, worst measured latency 2 fields, nothing over 2.** Per stage, `LoadLevel` issues one
read per stage and three stages in the same field; stage 12 issues no read, which is the census's
walk-in gate and the positive control that the tap is on CD reads. The 2 fields are the guest's own
per-field call cadence, not a host cost: the column is not ordered by byte size (524,288 bytes cost
0 fields once and 2 another time), which a byte-proportional host cost would be. The XA readiness
bit `[0x800774B4] & 0x40` was SET in 26 of 26 operations, so every load took the music-aware path
and it still cost at most 2 fields — but a constant reading cannot separate "always set" from "wrong
address", so R6 stays open with the named next step (a silent-music run must show it clear on some
operations). **(R6 is decided below, 2026-10-01: the bit IS clear in thousands of observations on
both cores, the instruction that clears it executed on the reference, and no read on this route was
issued while it was clear.)** Coverage is **20 of the 31 census sites, 11 unreached, 6 of the 20 unattributed by
site**: `$ra - 8` names a `jal` site only when the CALLER was guest code, and the port's native
`BootSequence::loadAssets` performs S02-S05 by dispatching the loader directly. Shown the other
answer: CTest `load_ledger` drives the real class with the completion withheld and the stage machine
stops at 2 operations, 1 pending, with the unfinished operation's latency reported as no latency at
all rather than 0.

**M1 and M2 are measured against retail (2026-10-01, `docs/issues/0155` "M1 and M2 measured, R6
decided", `tools/load_compare.py`, CTest `load_compare_selftest`).** Both cores were driven over the
same route in one run (`tools/oracle_spyro1.py`'s policy: save picker → `GS_Playing` → settled play
→ the twelve gameplay segments; native 7,738 frames, console 8,778 fields, 15 checkpoints, every
decisive range MATCH), the port through its REPL with `PSXPORT_LOAD_LEDGER`, the reference through
the pinned full-console core with its bounded PC observer on the two CD loaders (2,546,226,609
instructions scanned, 30 matches, 0 dropped, 0 pairing errors).

**M1: 28 of 28 payload operations identical, every SHA-256 included.** The reference's leg is not
read from the port: its loader arguments come from the observer's registers and its payload digest
is computed from `WAD.WAD` (110,260,224 B, sha256 `7ba8961c3626bcec`). Six rows differ in the
tuple's **issuer-site column alone** with byte-identical payloads, and every one is the known `$ra-8`
case: four reads the port dispatches from its own boot owner (site `0xDEACFFF8`) are the reference's
**S02-S05**, and the PETE read the port attributes to `0x8002D4A4` is **S06 `0x8005B83C`** — which
independently confirms §2.1's size-based identification of those five host-dispatched reads.
Coverage: **23 of the census's 31 issuer sites exercised on at least one core** (the port alone
reaches 18 named sites plus 6 unattributed operations), **8 unreached and named** — S07, S08, S09,
S10, S11, A17, A18, A19, every one a route this comparison does not take. M1's negative is measured,
not asserted: with the archive truncated so four reads lose their sectors, those four are reported
UNEQUAL with the byte range named and the tool exits 1 (`--truncate-bytes`; the exactly-one-sector
form is in the selftest).

**M2: 0 hand-off field differences at all four terminals** (three LoadCutscene stage-10 and LoadLevel
leaving stage 13), with the clock deltas named separately (`g_LevelTicks` 231 vs 435 at the boot
cutscene, 0 vs 1 at the level exit). At the first terminal — the only point where no later read has
re-used any destination — **all 28 payload destination ranges are byte-identical in both cores' RAM**;
at the later terminals the ranges differ because both cores have consumed their own staging buffers
(32 of 33 differing ranges have both cores past the payload; in the one exception the PORT still
holds its payload and the reference has moved on), so that column is reported rather than judged
outside the first terminal, and the tool prints which is which.

**R6 is decided: the XA bit is not permanently set, and the 26/26 was a property of WHEN reads are
issued.** Polled every field, `[0x800774B4] & 0x40` is CLEAR in 2,232 of 7,738 port observations and
3,441 of 8,778 reference ones (values `0x00`, `0x10`, `0x40`, `0x100`, `0x200`); `tools/writers.py`
finds the six immediate-form writers of that word, of which **two store `0x100` — bit 6 CLEAR** —
and the observer caught `0x8002BF2C` executing **2 times** on the reference. Yet at a loader entry
the bit was set in 28 of 28 on the reference too (the observer carries the word in every record, so
that is sampled at the read itself): Spyro issues its reads when the gate is open, which is why the
latency is bounded at 2 fields. R6 therefore stays open only in the narrower form "a read issued
while the bit is clear also costs nothing", needing a route with a music command in flight.

**M4 (absence of loading presentation) is still not done**, and the two logo holds remain. So this
item is **partial**, not verified: the payload, the terminal state and the residual latency are all
measured against the reference on this route, and the presentation claim has no measurement.

The logo-hold press latch is done (2026-10-01): a Start/Cross press made in any boot fade or in the
stage 3->10 loader is honoured at the next hold through the existing hold-skip route, measured as the
first hold 202 -> 0 fields (first-fade press) and the second hold 198 -> 0 fields (loader press) with
the fade and load-state field counts unchanged (`docs/issues/0155`, "Latch measured"; `press_latch`
in CTest). The flyby card and tally are authored and stay, cancellable.

### S034 — Spyro 2 loading removal

Missing. No load operation has been censused or classified for Spyro 2. Gap: enumerate its load
issuers and the wait and presentation each drives, then complete each through the title's own load
mechanics without its loading-only wait, with payload and terminal state compared against retail
and the absence of loading presentation captured.

### S035 — Spyro 3 loading removal

Missing. No load operation has been censused or classified for Spyro 3. Gap: enumerate its load
issuers and the wait and presentation each drives, then complete each through the title's own load
mechanics without its loading-only wait, with payload and terminal state compared against retail
and the absence of loading presentation captured.
