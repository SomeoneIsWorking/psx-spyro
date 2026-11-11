# Spyro 2 and 3 gameplay in-betweens on ReplayFrame

What it takes to replace `field_redraw` (the dispatcher redrawn at a blended camera, saved per flatten as producer
state) with route D (`psxport/docs/plans/replay-interpolation.md`): the per-field draw body replayed whole by a
`psx::present::ReplaySection`, with the moving inputs written by a `MotionSet`, as Spyro 1 does
(`titles/spyro1/core/frame/draw_section.*`, `docs/plans/spyro1-motion-tracks.md`).

Sources: `G2:<addr>` / `G3:<addr>` = Ghidra C in `scratch/decomp/spyro23-replay-s2all/c/<addr>.c` (SCUS_944.25) and
`scratch/decomp/spyro23-replay-s3all/c/<addr>.c` (SCUS_944.67), every function of each image decompiled; all bodies
present except game main (`0x80011ADC`, `0x8001200C`, an endless loop) and `G2:8001C1A8` (no body). Ghidra's
memory ends below the BSS globals (`0x80066FFF` on SCUS_944.25), so `--refs` answers "outside memory" for them: writers below
were found by grepping the decompiled C of every function. `port:` = existing port code. `RE:` = `docs/re-frontier.md`.
"inferred" marks a claim no decomp line shows. Kinds are Spyro 1's (`spyro1-motion-tracks.md` "Kinds"); the
moby and Spyro progress bytes are the nibble form of `game/render/field/keyframe_blend.h`.

## The section

Both draw bodies are one routine at two addresses (`RE: spyro2.field-redraw`, `spyro3.record`). The section is
the body minus its device tail. The device calls sit between the state switch and the flatten, so the replay
runs the switch and then calls the flatten directly, as Spyro 1's section skips its VSync tail.

| step | Spyro 2 (`G2:800156FC`) | Spyro 3 (`G3:8001E638`) |
|---|---|---|
| draw gate: return when set | `[0x800670C8] != 0` (cleared at the top of update `0x8001B140`; set by loads `0x800171BC`, `0x80019C40`, `0x8001A2FC`, `0x8001A79C`, `0x8001AF18`) | `[0x8006C718] != 0` |
| buffer flip: current DB word, DB0/DB1 (0x74 bytes each) | `0x80066FC0`; `0x80069928` / `0x8006999C` | `0x8006C600`; `0x8006FBFC` / `0x8006FC70` |
| primitive cursor = `[db + 0x70]` | `0x80067030` | `0x8006C664` |
| bin mark = `[table word] + 0x1000` | `0x80067168` = `[0x80066FFC] + 0x1000` | `0x8006C7D4` = `[0x8006C634] + 0x1000` |
| scratch end = cursor + `[size word]` | `0x80067034` = cursor + `[0x80067170]` | `0x8006C668` = cursor + `[0x8006C7DC]` |
| camera matrices from three angles | `0x8001C2F8(0x80067EC8, 0x80067E98, 0x80067E84)` | `0x8004F6C4(0x8006E03C, 0x8006E00C, 0x8006DFF8)` |
| camera position copy | `0x8001BDB0(0x80067EB8, 0x80067EAC)` | `0x8004F178(0x8006E02C, 0x8006E020)` |
| state switch on | `0x800681C8` | `0x8006E344` |
| gameplay handler | state 0: `0x80013A14` (clear colour, dispatcher `0x800155A0(0x3D)`, DEMO MODE text) | states 0 and 0xE: `0x8001D274` (same shape, dispatcher `0x8001E460(0x3D)`); 0x12: `[0x80074488]()` then `0x8001E460(0x3D)` |
| skipped: device and statistic | `0x8006717C` = scratch − cursor, DrawSync `0x800557E4(0)`, VSync `0x80058EDC` limiter (2 fields since last flip, one more when `0x80067044`), PutDispEnv `0x80055CA0(db + 0x5C)`, PutDrawEnv `0x80055BE0(db)` | `0x8006C7E8`, DrawSync `0x8005A264`, VSync `0x8005956C`, PutDispEnv `0x8005A71C`, PutDrawEnv `0x8005A65C` (inferred by position from Spyro 2's, which `RE: spyro2.record` names) |
| flatten, its head is DrawOTag's argument | `0x8001B2A8(0x580)`; empty table returns `0x8006012C`; tail linked to `0x8006012C` | `0x8004E664(0x580)`; empty returns `0x8006529C`; tail word kept in `0x8006C6B0` |
| second pass (not gameplay) | none | state 4 with `[0x8006C65C]` (Sparx) set, byte `0x80070134` set and more than 0xA00 bytes of scratch: walk pointed at Sparx (`0x8006C550` = Sparx, Sparx `+0xA0` = 0xFF), scratch from the other DB's pool, clear, walk `0x80030478`, drawer `0x80031124`, flatten again, chained through `[0x8006C6B0]`; state 0xD (`0x8006C4F8 != 2`): `0x8003CDA0` the same way |
| DrawOTag | `0x80055B70(head)` | `0x8005A5EC(head)` |

`linked(head)`: the frame's other chain is PutDrawEnv's packet inside the two DBs, so a head inside
`[DB0, DB1 + 0x74)` is not the section's, as in Spyro 1. The second pass is chained into the first, so a state-4
record still walks one section chain.

### What the gameplay handler calls

Dispatcher `0x800155A0` / `0x8001E460` with 0x3D (bits 1, 4, 8, 0x10, 0x20), in order:

| bit | Spyro 2 | Spyro 3 |
|---|---|---|
| 1 objects | `0x8004C534`: clear scratch, walk `0x80043858` (native), drawers `0x80044504`, `0x80046FD8`, close mobys `0x800499D4`; then while `[0x8006A250] == 0`: Spyro `0x80041DBC` (twice when `+0x2C` is set), shadow `0x800495E8`, `0x80048F04` while byte `0x8006AAB4` | `0x8001EC5C`: walk `0x80030478`, `0x80031124`, `0x80033C5C`, `0x8002DDA8`; while `[0x800705B0] == 0` (and not state 0xD before 2): Spyro `0x8003CDA0` (twice when `+0x2C`), shadow `0x8002D9BC`, `0x8002D2C4` while byte `0x80071834` |
| always | `0x800540B0` (cursor sprite on level 0x37 or while `0x80067114`) | `0x8002A580` |
| 4 terrain | `0x8004C4FC` → `0x80023BB4` (native) | `0x8001EC24` → `0x80022378` (native) |
| 0x10 effects | sprites `0x8004A8D4`, glows `0x8004AC78`, beams `0x8004B194`, trails `0x8004B72C`, meshes `0x8004BAA4` per live entry | `0x8002ECA8`, `0x8002F098`, `0x8002F5B4`, `0x8002FB68`, `0x8002FEE0` |
| 0x20 HUD | `0x80053E78` (native anchor overrides on its emitters) | `0x80029E48` |
| 8 sky | `0x80029170` (native), `0x8004C66C` | `0x8001C368` (native, while `0x8006C5B8`), `0x8001EDEC` |
| 0x10 particles | `0x80048128` | `0x8003AA38` |
| fades | `0x8004D748(2, s)` while `0x80066F68`; `(1, a)` while `0x80067150` | `0x800200A0` for `0x8006C598`, `0x8006C7C0`, red flash `0x8006C6D4` |
| letterbox | `0x8004D604` (last) | `0x8001FF44` (last) |

Devices: a walk of the decompiled call graph from the gameplay handler, the camera builders and the flatten
reaches 77 (Spyro 2) and 64 (Spyro 3) functions and none of DrawSync, VSync, LoadImage (`0x8005A478` in
Spyro 3), the CD or the SPU. The library leaves it reaches are SetDrawMode-style packet builders (`0x8005A01C`,
`0x8005C564`), string and GTE helpers and `rand` (below). Indirect calls (`state 0x12`'s pointer, the HUD's
`[0x80067158]`) are not followed (Open 4).

## Draw-side writes the replay must not leak

The sandbox restores them; the in-between's pre-draw value of an aged input is `pre_real + (t - 1) * step`
(Spyro 1's rule).

| state | address S2 / S3 | what the draw does | evidence |
|---|---|---|---|
| letterbox height | `0x8006701C` / `0x8006C64C` | ±1 per draw toward 12 while `0x80067100` / `0x8006C74C`, toward 0 after | G2:8004D604, G3:8001FF44 |
| red flash | — / `0x8006C6D4` | −0x20 per draw, floor 0 | G3:8001E460 |
| glow life | pool +0x0C | counted down by the delta `0x80067018` / `0x8006C648` | G2:8004AC78 (`_DAT_1f8003d4 = _DAT_80067018`), G3:8002F098; `port: effect_pools.h` |
| sprite spin | pool +0x12 | stepped by the draw | `port: effect_pools.cpp` `sprite::kSpin` |
| rand seed | `0x8006D144` / `0x80073F3C` | advanced by the beams drawer and the lives counter (`0x8004B194`, `0x80052B88` / `0x8002F5B4`) | G2:80059D2C, G3:8005C644 |
| clear colour | DB0 +0x19..1B, DB1 +0x19..1B (`0x80069941`, `0x800699B5` / `0x8006FC15`, `0x8006FC89`) | from the level colour `0x800681C0` / `0x8006E33C`, halved toward the fog colour `0x80067408` / `0x8006D07C` while the fog level `0x80067EC4` / `0x8006E038` is set | G2:80013A14, G3:8001D274 |
| Spyro's y, depth byte | `+0x08`, `+0x1E` | moved by −2·`[+0x2C]` and set to 0xF4/0xFA for the second Spyro draw, then put back | G2:8004C534, G3:8001EC5C |
| moby class byte, render and close lists, far-shade mark | record +0x4D; scratch; `0x80067168` / `0x8006C7D4` | the walk | `port: guest_moby_visibility.h` |
| sector visibility, terrain animation | `0x8006B300` / `0x80072330`; level sector words | the classification pass | `port: guest_terrain_animation.*` |
| `0x80048F04` scratch | `0x8006AA0C` = 0 | | G2:80048F04 |
| second pass (S3) | `0x8006C550`, Sparx +0xA0, `0x8006C668` | set and put back by the tail | G3:8001E638 |

The rand seed is restored, so an in-between draws S's random beams and lives jitter, not a blend: the beams flicker
between S' and S's patterns at 60 Hz exactly as they would at 30 Hz.

## Camera

| track | S2 / S3 | kind | read by | evidence |
|---|---|---|---|---|
| angles | `0x80067EC8` / `0x8006E03C`, three s16 | Angle12x2 at +0, Angle12Low at +4 | the matrix builder, first call of the body | G2:8001C2F8; `RE: guest.camera-builder` |
| position | `0x80067EAC` / `0x8006E020`, 3 x s32 | Linear | the copy, the walk, every drawer (`0x80067EAC..B4`) | G2:8001BDB0, G2:80048F04 |
| rotations, draw copy | `0x80067E84`, `0x80067E98`, `0x80067EB8` / `0x8006DFF8`, `0x8006E00C`, `0x8006E02C` | Derived (the section runs the builder and the copy) | | |
| scripted camera | gameplay `0x8001D8A4` copies `0x80068064/70` into the camera while byte `0x80067FC9` | nothing extra: lands in the tracks above | `RE: spyro2.record` |

`port:` Spyro 1's `DrawMotion::kCamera` is the same shape (`MotionTrack` with `Linear`, `Angle12x2`, `Angle12Low`).
field_redraw's `writeInBetween` wrote the matrices through `guest_terrain::cameraBetween` and raised the bin mark
past `deepestFarMark` because the terrain was not redrawn; with the terrain inside the replay neither is needed.

## Spyro

Record `0x80069FF0` / `0x80070328`, one layout (G3:8003CDA0 reads the same offsets as G2:80041DBC).

| track | layout | kind | evidence / port |
|---|---|---|---|
| position | +0x00, 3 x s32 | Linear | G2:80041DBC; `port: field_redraw.cpp:writeInBetween` |
| body angles | +0x0C, three bytes then one that is not | Angle8x3 | `FUN_8001B6C8(&+0xC)` at 800421B0; `port: anglesBetween` |
| head angles | +0x10 | Angle8x3 | 8004228C |
| body / head keyframes | +0x14 / +0x18: slot A, slot B, frame A, frame B | Pose, with the progress | 80041E30, 80041F40 |
| body / head progress | +0x1C / +0x1D, nibble form via `0x80062868` | Pose | 80041E34, 80041F3C; `port: keyframe_blend::between` (field_redraw never crosses an animation switch for Spyro) |
| depth bias | +0x1E | Step | G2:80041DBC:215 |
| tint | +0x20: r, g, b, amount (amount `>> 24`, 0 = off) | amount Scalar while both non-zero, colour Step | G2:80041DBC:487-493 |
| water floor | +0x28, s32, 0 = off | Step across 0, else Linear | G2:80041DBC:275,514 |
| reflection shift | +0x2C, s32 | Step across 0, else Linear (inferred) | G2:8004C534 |
| body matrix | +0x30..+0x40, 5 RT words; read only while the water floor is set | Matrix (`psx::present::rotationBetween`) | G2:80041DBC:523-527 |
| hidden | `0x8006A250` / `0x800705B0` | Step | G2:8004C534, G3:8001EC5C |

Tint, water floor, reflection shift and matrix are one rule, `SpyroSurface` (`game/core/draw_motion.cpp`): each blends
only while both samples have it on (tint amount non-zero; floor non-zero on the same side of 0, the fog branch being
the negative one; shift non-zero; the rotation, by `psx::present::rotationBetween`, while both floors are set). The
shift is `y - [0x8006AB78]`, written by update `0x8002A4C8`.

### Shadow and flame

| track | S2 / S3 | kind | evidence |
|---|---|---|---|
| shadow position | `0x800698D4` / `0x8006FBAC`, 3 x s32 | Linear | G2:800495E8:42-44, G3:8002D9BC; written by update `0x8002F7A8` from Spyro's position |
| shadow angles | `0x800698E0` / `0x8006FBB8`, three bytes, top byte a flag | Angle8x3 | G2:800495E8:52, G2:8002F7A8:25-43 |
| shadow hidden | `0x800698E3` / `0x8006FBBB` | Step | |
| shadow pose | Spyro +0x14, +0x1C | Derived (Spyro's track) | G2:800495E8 |
| flame anchor | `0x8006AA1C` / `0x8007179C`, 3 x s32 | `FlameMotion`: Linear, `to` when the lit count fell | G2:80048F04, G3:8002D2C4 |
| flame matrix | `0x8006AACC` / `0x8007184C`, 5 words | Derived: Spyro's draw copies his head or body rotation there while the aim byte `0x8006AAB6` is set | G2:80041DBC:273 |
| flame on, lit count, aim, path row | bytes `0x8006AAB4..B7` / `0x80071834..37` | Step; on and aim plus path row are the key | set by `0x8002FC20` while on is clear (on 1, count 0, aim 1, row random), aim cleared at timer 0x14 and on cleared when all 8 parts are done by `0x8002F08C` (S2); S3 lit by `0x800451C4` |

This is Spyro's flame breath: eight parts the update `0x8002F08C` anchors at his mouth and lights in turn
(`0x8006AAB5`). The anchor is the `flame` track, live while byte +0x98 is set and keyed by the aim and path row, so
a new breath never pairs with the last; the parts' timers step. Glimmer, Circle held: the breath grows from his mouth
across the in-betweens (`scratch/review5/flame2_strip.png`).

## Moby array

Pointer `0x80066F14` / `0x8006C550` (`port: render_facts.h kMobyFacts.firstRecordWord`), 0x58-byte records, the
walk visiting the last first; the list ends at the first record whose flag byte +0x48 is 0xFF, negative skips.

| track | layout | kind | port |
|---|---|---|---|
| position | +0x0C, +0x10, +0x14 | Linear | `moby_record::between` |
| angles | +0x44..+0x46 bytes; with +0x54 bit 30 on both sides, 4-bit fractions at bits 16/8/0 of +0x54 | Angle8x3, refined to 12 bits when flagged | `field_redraw.cpp:recordBetween` |
| keyframes, progress | +0x3C slot A, slot B, frame A, frame B; +0x40 nibble progress | Pose; across a switch only when `guest_moby::slotsShareLayout` | `keyframe_blend::between`/`across` |
| mesh | +0x36 u16 | identity, Step | `moby_record::sameObject` |
| flag, reach, depth bias, group, mirror bit | +0x48, +0x4C, +0x4E, +0x52, +0x54 bit 28 | Step | `guest_moby_visibility.h` |
| class | +0x4D | draw-written | |

Identity: same array pointer, same slot index, same mesh. No generation word (as Spyro 1, Open 3 there).

## Effects

All six pools are field_redraw's, with their pairing rules in `effect_pools.cpp:writeBetween`:

| pool | S2 / S3 base | records | pairing (port) |
|---|---|---|---|
| sprites | `0x80068078` / `0x8006E1F4` | 16 x 0x14 | kind and colour equal; position words Linear, spin Angle8 |
| glows | `0x80067D00` / `0x8006DEF8` | 16 x 0x10 | kind equal, life not rising; position Linear, life Scalar |
| beams | `0x8006B400` / `0x80072430` | 8 x 0x30 | segments and colour equal; both ends Linear |
| trails | `0x8006A2B4` / `0x80070610` | 32 x 0x30 / 0x6C, head byte +0x28 / +0x64 | owner and count equal; points paired by age |
| vertex meshes | `0x80069F30` / `0x80070260` | 8 x 0x14, each naming a −1-ended list of x, y, z words | list pointer and length equal; words Linear |
| particles | pointer `0x80066F18` / `0x8006C554` | 256 x 0x20, kind 0xFF ends | texture and kind equal, age not falling; points halfword Linear, size Scalar, spin Angle8 |

## Environment

| track | S2 / S3 | kind | evidence |
|---|---|---|---|
| sky sectors | `0x800681B8` / `0x8006E334` | Skip (level data; follows the camera) | `port: guest_sky_drawer.*` |
| sky objects | list `[0x80066F00]`, count `0x80066FA8` / `[0x8006C530]`, `0x8006C5E0`; position at +0x18, on byte +0x40 | Skip: level data, written only by the level setup `0x8001C734` and `0x80019928` | G2:8004C66C, G3:8001EDEC |
| sky object wave | tick `0x8006700C` / `0x8006C644`, +1 per tick (`0x80012488`, `0x800125C8` / `0x8003A40C`) | Step (Open 7): the draw reads the sine table `0x80061C58` at `tick * 4 + object * 0x40` and stores `(sine >> 4) + 0xE00` in `0x80068C08`/`0C0A` for `0x800296D0`, so a written tick cannot hold a half step | G2:8004C66C:178 |
| terrain animation, texture scroll | `0x800681E8..` / `0x8006E364..` tables | Step (applied per draw from level keys, as Spyro 1) | `port: guest_terrain_facts.h classify.animation` |
| fog level, colours | `0x80067EC4`, `0x80067408..10` / `0x8006E038`, `0x8006D07C..84` | Step (fog level is 0 or 0x800) | G2:8001E454:124 |

## Screen and HUD

| track | S2 / S3 | kind | evidence / port |
|---|---|---|---|
| subtractive fade | `0x80066F68` / `0x8006C598`, word (`lw`/`sw` at every site); ramps ±0x10 per update both ways (S2 `0x800159C8`, `0x8001FA58`; S3 `0x80053374`, `0x800144B4`), set by loads | Linear | G2:800159C8:30-33; Ghidra refs |
| additive fade | `0x80067150` / `0x8006C7C0` | Linear | no writer in the decompiled C (Open 5) |
| red flash | — / `0x8006C6D4` | Countdown: `0x8004B324` sets 0xFF at once, the draw lowers it 0x20 per draw, so a rise is shown whole | G3:8004B324, G3:8001E460 |
| letterbox | height and enable above | the body steps it, so inside a replay `screen_overlays::drawLetterbox` (`rowPerDraw`) keeps the bars' moving corners (+0x10, +0x14) on the row drawn and places them `t` of the way from the row read | G2:8004D604, G3:8001FF44 |
| conversation box | mode `0x80067134`, timer `0x80067130` (+ the field delta per update, `0x80016328`); state 1's handler `0x80013BC4` places the box at the halfword `0x800633F4[(timer - 0x18) / 2]` in mode 0 (timer 0x18..0x3B) and `0x80063418[timer / 2]` in mode 2 | `ConversationSlide` (`TableSlide`): the entry the later tick reads, written `t` of the way from the one the earlier read | G2:80013BC4:22-45 |
| HUD tick | `0x80067008` / `0x8006C640`, +1 per update | Step (`(tick >> 1) % 6` picks the gem frame) | G2:80053E78:11-12, G2:800159C8:12 |
| HUD shown | letterbox enable and height both 0 | Step | G2:80053E78:7 |
| demo text | `0x80067024`, `0x80067118` / `0x8006C658`, `0x8006C76C` | Step | G2:80013A14 |

Spyro 2's HUD counters are 8-byte slots from `0x8006765C` (state byte, slide halfword +2, value +4), stepped by
`0x8005199C`; `0x80052498(slot, offset, y)` returns the slide `0x80063430[(slide + offset) / 2]` (state 1) or
`0x80063448[...]` (state 4), the index clamped to 0..0x2F, else 0x28, negated below y 0x78, and each digit passes its
own offset (−3 per digit), so no table entry can be written for all readers. `HudSlide` (`game/core/hud_slide.*`)
samples the six slots and is the function's native override: it returns the guest's result, and inside a replay
(`ReplayFrame::replaying`) `t` of the way from the result for the earlier sample's slot word while both samples draw
the slot. Spyro 3's widgets slide by another path (Open 8).

## Cuts

`spyro::FrameCut` already decides them for these titles (`game/core/frame_cut.*`, through
`BootPrefixRuntime::sealedFrameIsCut`): game state (level-load fades counted as the field), level id, the camera
controller's three placement steps (`0x80067ED0`/`0x80067ED4`, `0x8006E044`/`0x8006E048`) and a cutscene key
track's shot change. An interval with a cut sealed in it is held as drawn even when a replay is present
(`FramePresenter::presentRecords` tests `cutBetween` before `covers`), so the replay needs only `forget()` where its
sample breaks: a frame whose draw gate is set or whose state the section does not replay, and a state load.
Warps (`titles/spyro2|3/core/warp.*`) go through the guest's own load, so the level id cuts them.

## The frame driver seam

`spyro::BootPrefixFrameDriver` (`game/core/boot_prefix_frame_driver.*`) runs update and draw as resumable guest
calls. The point equivalent to Spyro 1's `replay_.beforeDraw(core)` is `stepMainLoop` just before
`enterCall(facts_.frameDraw)`: the update has returned and the draw has not started. There:
`replay.beforeDraw(core)` when the gate is clear and the state is one the section replays, `replay.forget()`
otherwise; `restored()` forgets. `BootPrefixRuntime` overrides `replayFrame(Core&)` as Spyro 1's runtime does.
The draw suspends in its VSync limiter after the replay ran; the record that seals at the suspension walks no
table (`drawsAnything` false, `docs/issues/0177`), so `ReplayFrame::sealed` hands the captures to the next
record, the one that walks, as wanted.

## What changes

1. psxport `present/motion_set`: fixed-address `MotionTrack`s cannot name the moby array (behind a pointer, length
   set by an end byte) or the effect pools' pairing. Add one abstract title-owned input beside the tracks
   (`MotionSource`: `sample(Core&)`, `forget()`, `write(GuestRedraw&, t)`), owned by `MotionSet` so pairing and
   forgetting stay one rule. Spyro 1's moby array, particles and sparkles (its plan's open rows) take the same seam.
   Check `codemap.py find` for a pool sampler first.
2. psx-spyro `game/`: the shared section as a base class `spyro::FieldDrawSection : psx::present::ReplaySection`
   (preamble, camera builder and copy, flatten, `linked`) over the image facts above, with title subclasses in
   `titles/spyro2|3/core/frame/` owning the state switch (and Spyro 3's second-pass tail) by guest address. The
   moving inputs as one `MotionSource` built from field_redraw's code moved, not copied: `readInputs`,
   `recordBetween`, the Spyro block of `writeInBetween`, `effect_pools::read`/`writeBetween`, plus the new rows
   above (Spyro tint, water, shift, matrix; shadow; flame; fades). Camera as two `MotionTrack`s.
3. Overrides inside the replay: steps are dispatched with their overrides in force (`GuestRedraw::dispatch`).
   Every S2/S3 override that saves producer state or keys packets would run inside the sandbox: the dispatcher's
   `field_redraw::redrawField` (it would recurse), the screen draws, the terrain producer, the tint and letterbox
   producers (`screen_overlays.cpp`, which skip while `fieldRedraw.redrawing`), Spyro 2's conversation producers
   and the loading picture producer. With a replay title the presenter composes no producer state, so they become
   plain native overrides (the widescreen work they do stays) or go: `field_redraw`'s History, redraw, `placeSky`,
   `passOverKeyed`, `fieldClaim`/`saveField`/`ownSecondPass`, `FieldRedrawProducer`, `Session::fieldRedraw` and
   `secondPass`, the screen-draw and second-pass facts and Spyro 3's `codeLanded` install of them. The flatten
   override keeps naming the bins in `core.otTables` (the replay ends the walk it starts).
   `TerrainStateProducer`, `terrain_rebuild`, `cameraBetween` and the terrain drawer's in-between mode went with
   them.
4. All field-drawing states at once: the presenter holds every interval a replay title does not cover
   (`frame_presenter.cpp`, `HeldCause::NotReplayed`), so replaying gameplay alone would freeze the title screen,
   cutscenes, conversations, level-load fades and the pause menu at 30 Hz where field_redraw moves them today. The
   switch is in the section and the inputs are the same in every state, so each handler is replayed, and each
   state shown before the change lands. A state whose handler reaches a device (Spyro 3's story pictures,
   LoadImage `0x80074D28`; `UpdateLoadingImage` `0x80058778`) refuses and holds, recorded as an issue.

The terrain in-between problems of issues 0230 (reads of level data the game has since overwritten) and 0235
(keyed packets carrying another writer's store) do not arise: the replay runs inside its own logic frame on live
data and no packet is keyed.

## Proof

- Builds: psxport, then `spyro_port`, under the heavy lock.
- Spyro 2 Glimmer and Spyro 3 Sunrise Spring through their warps, judgement settings, unpaced, camera turning and
  Spyro running (held buttons); crops of camera pan, Spyro, a walking moby, a glow and particles across presents
  (`scratch/replay2/blink.py` adapted): each in-between halfway, nothing missing. `replay:warn` 0, no
  `IntervalHeld` during play. One paced run each through `vblank.py`.
- The same for the title screen, a level-load fade, a cutscene, a Spyro 2 conversation and the Spyro 3 pause
  menu (Sparx's second pass), each reached by its warp or title route.

## State (2026-11-10)

Built: psxport `MotionSource` (a title-owned input beside the tracks, sampled and written by the `MotionSet`);
`game/core/draw_section.*` with `titles/spyro2|3/core/draw_section.*` (the switch above, S3's second pass,
state 5 not replayed); `game/core/draw_motion.*` (camera angles and position, Spyro, his poses, his shadow, the
moby array through `MobyMotion` — field_redraw's `recordBetween` moved — and the effects pools through
`EffectMotion`); `BootPrefixFrameDriver::beforeDraw` before each draw entry, forgetting when the gate is set, the
state is not replayed or changed, and on restore. Deleted: field_redraw, the terrain state producer and rebuild, the
loading picture and conversation producers, the screen-draw installs, the flatten's claims and second-pass table.

Shown (hidden, unpaced, judgement settings, Up and Left held): Glimmer `warp 0/1/0` pans 40..41 px of 5472 at every
present and pairs 218 of 218 moby records; Sunrise Spring `warp 0/0/0` pans 41..43 px and pairs 212 of 212; Spyro's
wings and tail and Sparx land between their neighbours (`scratch/replay23/s2b/spyro.png`, `s3a/tail.png`). No replay
refusal in gameplay. Open: issue 0237 (loading screen held, untracked fades and letterbox, other states unjudged,
the terrain drawer's dead in-between mode, paced judgement on a quiet host).

Added (2026-11-10, second pass): `SpyroSurface`, the flame breath, the fades (S2 `0x80066F68`, `0x80067150`; S3
`0x8006C598`, `0x8006C7C0`, red flash `0x8006C6D4`) and Spyro 2's conversation box slide. Shown (hidden, unpaced,
judgement settings, `scratch/spyro23-tracks`): the Glimmer arrival fade-in alternates t=0.5 / t=1 with each in-between's
centre brightness between its neighbours (6.24 / 9.35 / 13.30, 13.30 / 17.41 / 21.95, 21.95 / 27.20 / 33.32); the
conversation box (`warp 0/1/0`, `moby-teleport 554 2`) slides 128 of 5472 columns per present, every in-between
halfway, through its bounce (`conv2_box.png`); the Spyro 2 title screen's Spyro turns his head halfway
(`title2_spyro.png`); Spyro 3's pause (`warp 0/0/0`, Start) composes every in-between with Sparx's second pass drawn
and his wings between their neighbours (`pause3_sparx.png`), while its HUD slide steps (Open 8, `pause3_hud.png`).
Spyro 3's warp no longer refuses a replay (Open 12). Nothing leaks: warp, Up and Left held 600 steps, fps60 on
(Spyro 2: 920 in-betweens, Spyro 3: 746) and off (0, since psxport replays nothing without fps60): the `fielddigest`
lines are byte-identical, 2914 and 2813 fields. Re-run 2026-11-11 (`scratch/review5/leak-spyro{2,3}-{0,1}.dig`): the
same counts, identical; the declined frames report as S2 `state 5 polls 0x800577A0 for its pages` (16), S3 `state 5
waits in VSync at 0x8005927C` (253), `state 3 clears both buffers through 0x8001EBAC` (1).

Added (2026-11-11, `scratch/hudslide`, hidden, unpaced, judgement settings): `ReplayFrame::replaying` hands title
overrides the fraction they draw at. Spyro 2's HUD slide (`HudSlide`): Glimmer `warp 0/1/0`, Select: the gem icon's
lowest row per present 127 / 151 / 167 / 183 / 207 / 231 / 247 of 1920 (t=1 rows step 40, the in-betweens 24 or 16
past the earlier: the nearest whole native row to halfway), the bounce likewise (`s2_gem.png`); the override returns
the guest's own result in 343 of 343 calls checked against the original outside a replay. Letterbox (`rowPerDraw`):
`moby-teleport 554 2`, both bars grow 12.0, 12.5, 13.0 ... 24.0 native rows, half a row per present, across the
widened margins too (`letterbox.png`). Water: Summer Forest `start`, Up held into the stream: the water floor
`+0x28` is set and every in-between draws Spyro's water pass composed, his strokes and Sparx between their
neighbours (`sfwater_seq.png`); tint and reflection shift stayed 0 on every route tried. Crush's Dungeon
(`Summer Forest/Crush's Dungeon/start`, state 0): Crush and the camera move halfway (`boss_seq.png`). Leak: Select,
`moby-teleport 554 2`, Up and Left, fps60 on (906 in-betweens) and off: `fielddigest` identical over 2888 fields;
Spyro 3 Sunrise Spring, Up and Left (746 in-betweens), identical over 2813.

## Open

1. Done: the letterbox edge (Screen and HUD). A bar that shrinks to 0 is not drawn by S, so its last half row is not
   shown. Spyro 3's facts share the body's shape (G3:8001FF44) but no warp reached a Spyro 3 letterbox.
2. Angle8 quantisation of moby and Spyro angle bytes, as Spyro 1 Open 2; mobys flagged with bit 30 have
   fractions, Spyro and the shadow have none.
3. Whether any gameplay chain bakes a VRAM upload (`ReplayFrame::at` refuses such a record); none reached by the
   call graph, not measured.
4. Indirect calls in the section (`[0x80074488]` in Spyro 3 state 0x12, the HUD's `[0x80067158]`, state 2's
   pointers) were not followed for device access.
5. `0x80067150` (Spyro 2 additive fade) has no writer in the decompiled C; it may be written through a pointer or
   by overlay code.
6. Done: the flame breath (`FlameMotion`).
7. Sky objects: level data (Environment table). Their wave steps at 30 Hz. The table is the general sine read by
   other drawers of the same section (`0x8001B664`, a callee of `0x8004C66C`, and the particles `0x80048128`), so its
   entries cannot be rewritten for the replay. The faithful path is an override of the consumer `0x800296D0` that, in
   a replay, rewrites `0x80068C08`/`0C0A` from the sine `t` of the way between `(tick - 1) * 4` and `tick * 4` (a whole
   entry at t = 1/2); it needs the object index, which lives only in `0x8004C66C`'s frame (`local_90`), so it is not
   done.
8. HUD slides: Spyro 2 done (`HudSlide`). Spyro 3's still step (`scratch/spyro23-tracks/pause3_hud.png`): its widgets
   (`0x80029E48`, drawers `0x8002982C`, `0x80029904`, `0x80029A00`, `0x80029AA0`, `0x80029BB0`, `0x80029CF8`) keep
   the slide in byte +0x3F, and the table `0x800674E8` is read inline in `0x80029708` (icon, index
   `(slide + 10) / 2` clamped to 0..0x3B) and per digit in `0x800291B8` / `0x800293C4` (from the value `0x80029708`
   leaves in its index argument). There is no one function whose result is the offset, so the blend needs overrides
   of `0x80029708` and the two digit drawers that carry the earlier sample's slide from the widget down to the digits.
   Spyro 3's HUD tick `0x8006C640` is matched to Spyro 2's by use only (writers `0x80053374`, `0x80050F18`).
9. Pause menus (`0x800145AC`, `0x8001DD1C`): Spyro 1's pause reads VRAM back; whether these do is not known.
10. Spyro 2's conversation slide: done (`ConversationSlide`). The loading pictures' fade counters (`0x800698EC`,
    `0x8006FBC4`) belong to state 5, which is not replayed.
12. Spyro 3's level load, state 3 handler `0x8001DC5C`: in steps 0 and 4 of `0x8006FBC8` with the fade at 0xFF it calls
    `0x8001EBAC` (DrawSync `0x8005A264`, whose GPUSTAT poll at `0x8005C224` never ends in the sandbox, VSync, two
    ClearImage `0x8005A3E8`), so `spyro3::DrawSection::declinesState` declines that frame; the screen is black there.
    Each declined frame is counted by its reason and reported at run end (`BootPrefixFrameDriver::reportDeclined`).
11. Cost: each replay runs the whole field, terrain included, once per in-between; not measured against
    field_redraw's dispatcher redraw plus terrain rebuild.
13. Native depth (psxport `docs/presentation.md` Depth), Spyro 2 and 3 alike (`engine_render_overrides.h`):
    - Terrain `0x80023BB4`'s near pass is `Polygon` with a 32-unit bin. The far LOD pass
      (`guest_terrain_far.cpp`) projects with no depth: its coarse quads overlap each other and drew notches into
      near tree trunks at Spyro 2 `warp 1/0/0` (580 px off against on, 8 after).
    - Spyro's drawer (`0x80041DBC` / `0x8003CDA0`, `spyro_drawer_depth.cpp`) is `Polygon` with 4-unit buckets. It
      opens his view unit (1/4, translation world minus camera) and his bias byte +0x1E in bins. The reflection call
      (+0x2C set) and Spyro 3's second pass (state 0xD, return `0x8001EB3C`) project with no depth.
    - The moby drawer (`0x80044504` / `0x80031124`) is `Object`. The walk tags each entry's centre z with
      `meshDepthScale` and its sorted-nearer bins (`guest_moby_visibility.cpp:finishEntry`), and the drawer's
      `CTC2` of TRZ carries that unit.
    - Shown (`scratch/depth23`, player settings, unpaced, off against on): Glimmer `0/1/0` holding Up+Left 200
      fields (Spyro 2), a grey wall top no longer pops over the near green overhang, on real presents and in-betweens
      (`h_top.png`, `h_top0.png`). Spyro 3 Sunny Villa `0/0/0` and `0/1/0` (41 px) and Cloud Spires `0/2/0` (134 px,
      Spyro only) show no holes.
    - Not done: a moby still draws over terrain in front of it, because `Object` never tests. Marking it `Polygon`
      needs each face's bias: the drawer subtracts the face word's bits 0-2, sign-extended, << 2 << (detail + 4)
      from the summed SZ (80044DAC..DBC). Spyro's drawer has the same bias, 128 summed SZ per step (80042C64).
      Both live only in `$s0` mid-body while the packet's SXY words are stored from GPRs. Carrying them needs a
      native drawer, or a vertex-place hook at the face store.
    - Unmarked, so slot order: the deferred list `0x80046FD8` / `0x80033C5C`, the close mobys `0x800499D4` /
      `0x8002DDA8` (each computes its own TR), the shadow `0x800495E8` (translucent). Their units are not derived yet.
