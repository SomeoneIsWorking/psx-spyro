# Spyro 1 motion tracks for the replayed gameplay draw section

Inputs the section reads that change between logic frames, for route D
(`psxport/docs/plans/replay-interpolation.md`). The section is GamestateDraw `0x8001ED5C` GS_Playing:
`0x80033C50`, `0x800521C0`, `0x80019300` (not flight), `0x80018908` (demo), `0x80019698`, `0x8002B9CC`,
`0x80050BD0`, `0x800573C8`, `0x800190D4`, `0x80018F30`, `0x800189F0`, `0x80016784(0x800)`.

Sources: `ext` = `external/spyro-1/` (`src/`, `include/`, `asm/`); `G:<addr>` = Ghidra C in
`scratch/decomp/spyro1/c/<addr>.c` (all bodies present); `port:` = existing port code. "inferred" marks a
claim not shown by a decomp line or Ghidra output.

## Kinds

| kind | rule |
|---|---|
| Vector | signed 32-bit words, lerp |
| Angle12 | halfword, wrap 4096, short arc |
| Angle8 | byte, wrap 256, short arc; the guest indexes a 256-entry sine table with the byte, so sub-byte phase is lost (Open 2) |
| Scalar | lerp, wrap noted |
| Pose | animation pair `(anim, next anim, frame, next frame, progress)` blended as one unit: `game/render/field/keyframe_blend.h` `between`/`continues`/`across`; never lerp the index bytes alone |
| Step | take the newer value; never lerp |
| Matrix | RT words, slerp (`psx::present::rotationBetween`) |
| Derived | recomputed by the section from other tracks; write nothing |
| Skip | not read by the section |

Moby, particle and sparkle records are 16-bit or 8-bit fields: write each blended value back with its own
width.

## Draw-side writes the replay must not leak

The section mutates state. A replay starts from the pre-draw value and the sandbox restores it; the
in-between's pre-draw value for an aged scalar is `pre_real + (t - 1) * step` (inferred from the order:
the draw adds one step before reading).

| state | address | what the draw does | evidence |
|---|---|---|---|
| portal yaw | `0x80075858` | `= (v + 2) & 0xFFF` once per draw, before use | G:80050BD0 first lines |
| portal pitch | `0x800758FC` | `+= 2`, wrap past 0x800, clamp to 0x80 | G:80050BD0 |
| border height | `0x800756C0` | `+= g_DeltaTime` while `g_ScreenBorderEnabled`, clamp 22; `-=` otherwise, floor 0 | ext `draw.c:429-441`; refs: only `0x80018F30` writes it besides load `0x8001364C` |
| sparkle ageing | `0x80077108` x8 | fade and angle step by `g_DeltaTime` (`0x800756CC`) passed to `0x800584C4` | G:80058BA8, `port: glows.h` |
| tracer count | `0x80075684` | set to 0 at the end of `0x800189F0` (list consumed) | ext `draw.c:425`; refs show the only writes are load and `0x800189F0`; adders are in overlays |
| particle seen byte | record +3 | set when the record passes the view tests | `port: particles.h` |
| moby `m_WasDrawn` | moby +0x51 | cleared and set by `0x800521C0`/`0x8001F158`; read by moby logic | G:800521C0, G:8001F158 |
| sector marks | `0x800771C8` | the terrain pass writes it; `0x800521C0` (earlier in the section) reads it, so it is one draw stale | G:800521C0 (`DAT_800771c8[sector]`), `port: terrain_facts.h` sectorMarks |
| terrain sector animation | sector +0x18..+0x1B, level sector data | the classification pass applies each mark under 0x80, writes the level's vertex/colour words, sets the mark to 0xFF | `port: game/render/terrain/guest_terrain_animation.h` |
| flame view rotation | `0x80078780..0x80078790` | written by Spyro's draw when `0x80078762` is set, read by `0x80058D64` | G:80023AC4 (`_DAT_80078780 = DAT_80071604 ...`) |
| shadow list end, pool flag | `0x80075F00`, `0x800758B0` | cursors | G:8001F158, G:800573C8 |

## Camera

`g_Camera` = `0x80076DD0` (ext `camera.h`).

| track | address / layout | kind | identity | read by | evidence |
|---|---|---|---|---|---|
| position | `0x80076DF8` (+0x28), 3 x s32 | Vector | single object | every drawer (`0x80076DF8..0x80076E00`), 8001F158, 80059F8C, 80059A48, 80050BD0 | G:8001F158, G:80023AC4:186; already used |
| rotation | `0x80076E1C` (+0x4C), 3 x s16: x, y, z | Angle12 | single | `0x80033C50` (builds both matrices), 80050BD0 (yaw/pitch/roll terms) | ext `camera.c:28-86`; already used |
| projection matrix | `0x80076DD0` (+0), 5 words | Derived | | 8001F158, 80022A2C, 80059F8C, 80023AC4, 800573C8 (copied into GTE) | `0x80033C50` writes it from rotation (`camera.c:84`); row 1 scaled 320/512 |
| view matrix | `0x80076DE4` (+0x14), 5 words | Derived | | terrain classify, `0x8004EBA8` | `camera.c:80` |
| occlusion group | `0x80076E24` (+0x54), s32 | Step | single | `0x8002B9CC` (culling distance, group), `0x80050BD0` (final `0x8004EBA8` argument) | ext `environment.c:24-40`, G:80050BD0 tail |
| metal matrix | `0x800770E0`, 5 words (`D_800770C8 + 0x18`) | Derived | | `0x80020F34` (`_DAT_800770e0..f0`) | ext `specular_and_metal.c:7`: built in update from the camera's view matrix before `CameraUpdate`, i.e. from the previous draw's matrix; rebuild with the guest's `MetalBuildMatrix` after `0x80033C50` in the replay (inferred; address not resolved, Open 7) |

Other camera fields (`m_DestinationPosition` +0x34, spherical fields, `m_State`, `unk_0xCC`) are not read by
the section: Skip. The unused screen shake (`0x8007590C`, `0x800756DC`) adds to the position in update only
(ext `camera.c:577-590`).

## Spyro

Record `g_Spyro` = `0x80078A58`, one object (ext `spyro.h`). Offsets below are from it.

| track | layout | kind | read by | evidence |
|---|---|---|---|---|
| position | +0x00, 3 x s32 | Vector | `0x80023AC4`, `0x80059A48` indirectly | G:80023AC4:186-188 |
| body / head / tail angles | +0x0C / +0x10 / +0x14: bytes x, y, z, then a pad byte | Angle8 | `0x80023AC4` (word `_DAT_80078a64/68/6c`, byte*2 into the sine table) | G:80023AC4:233,330,363; `port: actor_facts.h spyro_record` |
| animation pairs | +0x18..+0x1D: body anim, next, head anim, next, tail anim, next; +0x1E..+0x23: body frame, next, head frame, next, tail frame, next | Pose (per part) | `0x80023AC4`: `model[0].anims[anim]`, frame word at `anim + 0x24 + frame*8` | G:80023AC4:86-141; model = `g_Models[0]` (`0x80076378`) |
| part progress | +0x24 body, +0x25 head, +0x26 tail; 0..15, 16 steps, weight `k << 8` over 0x1000 | Pose (with the pair) | `0x80023AC4`, `0x80059A48` (body) | G:80023AC4:114,131; `port: keyframe_blend.h` |
| depth bias | +0x27 | Step | `0x80023AC4` (`DAT_80078a7f`) | G:80023AC4:208 |
| tint | +0x28: r, g, b, amount | amount Scalar 0..255, colour Step | `0x80023AC4` (`_DAT_80078a80`, amount `>>24`) | G:80023AC4:697-702 |
| swim water floor | +0x30, s32; 0 = off | Step across 0, else Vector (1 word) | `0x80023AC4` (`iVar38 = _DAT_80078a88`) | G:80023AC4:86 |
| body orientation | +0x34..+0x44, 5 words (`m_RotationMatrix`) | Matrix; only matters while the water floor is non-zero | `0x80023AC4` water cut (`_DAT_80078a8c..9c`) | G:80023AC4:659-664 |
| hidden | `g_IsSpyroHidden` `0x80075814` | Step | `0x80019698` (skips `0x80023AC4`, `0x80059A48`) | G:80019698 |
| head rotation matrix | +0x1C8 (`m_headRotationMatrix`) | Skip | no read of `0x80078C20..` in the section | G:80023AC4 reads no 0x80078Bxx+ |

How the guest blends a part: A = `(anim, frame)`, B = `(next anim, next frame)`, weight = progress / 16. Per
tick (`0x8003CB24`): `progress += step`; past 15, `progress -= 16`, `anim = next anim`, `frame = next
frame`, `next frame += 1`, wrapped to the animation's loop start (`0x8006C4A0[anim*4]`) at its end
(`0x8006C4A1[anim*4]`). Head (`0x80049660`) and tail (`0x80049E8C`) copy the body's pair and progress
unless transitioning (head `+2`/`+4` per tick). Evidence: G:8003CB24, G:80049660, G:80049E8C. The guest
cross-blends Spyro's own animation switches, so his pairs always continue
(`docs/issues/0196-gameplay-in-betweens-judder.md`, "Spyro 1 poses").

Angle source: the bytes are `physics angle >> 4` (`func_8004AC24` sets `0x80078B74/78/7C = byte << 4`;
decomp `m_SpeedAngle.{m_RotZ,m_RotY,m_RotX}`), so the 12-bit values exist at `0x80078B74..0x80078B7C` for
the body (G:8004AC24). Head and tail angles have no 12-bit source found (Open 2).

### Spyro's effects

| track | address / layout | kind | identity | read by | evidence |
|---|---|---|---|---|---|
| shadow | `D_8007AA10`: +0x10..+0x18 position (3 x s32, `0x8007AA20..28`), +0x24 over-water (`0x8007AA34`, skip when > 0), +0x00..+0x0C shade words | position Vector; rest Step | single | `0x80059A48`; also reads body pair and progress `0x80078A70/76/7C` and angle byte `0x80078A66` | G:80059A48:28-66, ext `spyro.h SpyroShadow` |
| flame block | `0x800786C8`, 0xCC bytes: anchor +0x00 3 x s32; part lengths +0x20, 8 signed bytes; limits +0x28; active byte +0x98 | anchor Vector; lengths Scalar when the part grew and the limit is unchanged, else Step; active Step | single | `0x80058D64` (called when `0x80078760` != 0) | `port: flame_block.h`, `flame_producer.cpp:blockBetween`, G:80058D64 |
| flame view rotation | `+0xB8`, 5 words | Derived (written by `0x80023AC4` from Spyro's blended angles) | | | G:80023AC4 `_DAT_80078780..90` |

## Moby array

Base pointer `g_LevelMobys` at `0x80075828`; slots are 0x58 bytes, contiguous; the level's mobys come
first, the dynamic pool starts at the pointer in `0x80075890` (`g_DynMobys`). There is no count: the walk
stops at the first slot whose state byte (+0x48) is 0xFF. State bit 7 set and not 0xFF is dead and
skipped (0xFE released, 0xFD below the pool). Evidence: G:800521C0 (`iVar5 = _DAT_80075828 - 0x58`, stride
0x58, `(uVar9 << 0x18) < 0`, `== 0xFF` ends), `port: core/moby/native_moby_allocator.cpp` (marks, slot
stride, allocation cursor `0x8007573C`).

Each live slot is binned by its renderer byte (+0x4B): bit 7 set to the specular list (`0x80071EF4`, drawn
by `0x800208FC`/`0x80020F34`); byte 0 to the queue (`0x800720F4`, `0x80022A2C`, lit and HUD mobys); else the
primary list (`0x8006FCF4`, `0x8001F158`/`0x8001F798`). A slot whose sector byte (+0x4A) is unmarked is
dropped and its +0x51 cleared.

| track | layout | kind | read by | evidence |
|---|---|---|---|---|
| position | +0x0C, +0x10, +0x14, s32 | Vector | `0x8001F158` (entry centre), `0x80059F8C`, `0x80022A2C` | G:8001F158 (`*(puVar30 + 0xc)`...), G:80059F8C |
| angles | +0x44, +0x45, +0x46: bytes x, y, z; +0x47 is the depth offset, not an angle | Angle8 | `0x8001F158` rotation compose; `0x80022A2C` | G:8001F158 (`*(puVar30 + 0x44)`, sine table `0x8006CBF8`); `port: moby_record.h` |
| pose | +0x3C anim, +0x3D next anim, +0x3E frame, +0x3F next frame, +0x40 progress (0..63, weight `k << 6`) | Pose | `0x8001F158`: `model + 0x38 + anim*4`, frame word `+0x24 + frame*8`, radius `+7`, depth scale `+0xB` | G:8001F158, ext `moby.h AnimationState`/`AnimationHeader`; `port: draw_motion.cpp:MobyMotion` |
| progress step | +0x41 (per frame), +0x42 (flags) | Skip (update only) | `0x800522C0` | G:800522C0: `progress += perFrame >> shift`; at 64 the pair advances, `next frame += 1`, wrap at `numFrames` (flags set), animation switch when `anim != next anim` |
| class | +0x36 u16 | Step; identity word | all | G:8001F158 (`g_Models[class]`) |
| state | +0x48 | Step | `0x800521C0` (alive) | above |
| depth offset | +0x47 (s8) | Step | `0x8001F158` flags word | G:8001F158 (`uVar17 >> 0x18`) |
| renderer / distance | +0x4B: bit 7 shiny, low 7 bits distance; low 6 bits also the depth bias | Step | `0x800521C0` (list choice), `0x8001F158` (`& 0x3f`) | G:800521C0, G:8001F158 |
| palette word | +0x4C..+0x4F: colour bytes, type | Step | `0x800208FC` (copied to the entry), `0x80022A2C` (`>> 22`) | `port: actor_list.h copiesSecondaryWord`, `sprite_queue.cpp:153,363` |
| render radius / cull | +0x50 u16: low byte `<< 8`, bit 8 adds 0x200, bit 7 of the byte pair marks a screen-space (HUD) record | Step (never lerp across a HUD flag change) | `0x8001F158` cull, `0x80022A2C` | G:8001F158 (`*(ushort)(puVar30 + 0x50)`), `port: actor_facts.h kScreenSpace` |
| scale | +0x57; 0 = regular size, else `scale / 32` | Scalar when both frames are non-zero, else Step | `0x8001F158` flags word low byte; emitter scales the centre and radius | G:8001F158 (`puVar30[0x57]`), `port: actor_emitter.cpp:86-93` |
| shadow word | +0x1C: low 16 = shadow distance, bits 16..21 and 22..27 = tilt angles (64 per turn, byte index `* 8` into the 256 table) | distance Scalar; tilt Angle8 scaled by 4; sign bit (word < 0) stages the shadow, Step | `0x8001F158` (stage), `0x80059F8C` | G:8001F158 (`*(int)(puVar30 + 0x1c) < 0`), G:80059F8C (`uVar12 & 0xffff`, `>> 0x16 & 0x3f`, `>> 0x10 & 0x3f`); tilt scale inferred |
| shadow frame size | frame `+0x2A` of the model animation | Step (model data) | `0x8001F158` | G:8001F158 |
| rotation matrix | +0x20..+0x33 (`m_RotationMatrix`) | Skip (collision only) | no read in the section | G:8001F158 reads none of +0x20..+0x33 |
| drawn flag | +0x51 | draw-written, see above | | |
| spawn id | +0x56 `m_MobyIndex` | Skip (gem bitmask) | | ext `moby.h:159` |

Identity across two logic frames: slot address plus class (+0x36). The level region never reallocates, so
address plus class is exact there. In the dynamic pool nothing in the record separates a freed and
re-allocated slot of the same class (no generation word; the props pointer at +0x00 comes from a downward
allocator and can repeat). Rule: same address, same class, same props pointer, state alive in both; a
position or angle jump past a per-class limit is a cut (inferred; Open 3). Sparx is a normal record of class
120 (`MOBYCLASS_SPARX`, `g_Sparx` pointer; `moby.h:272,404`); its glow is the glow row below.

HUD mobys are not in this array: `g_Hud` = `0x80077FA8`, `m_Mobys[12]` at `0x80077FEC`, listed into the
queue by `0x80019300`; text mobys are rebuilt each draw from the arena at `g_HudMobys` `0x80075710`
(`0x800181AC`/`0x80017FE4`) and carry no state between draws.

## Effects

| track | base, stride, end | fields | kind | identity | read by | evidence |
|---|---|---|---|---|---|---|
| particles | pointer `g_Particles` `0x80075824`; 0x20-byte records, 256 slots (`0x2000` span); the list ends at the first record whose kind byte is 0xFF; 0xFE (released) is skipped | +0 texture group, +1 kind, +2 life (ticks left), +3 seen (draw-written); x, y, z halfwords at +4, +6, +8; turned kinds (2, 5, >= 6) angle byte at +0x0B; line kinds (1, 4) end point halfwords at +0x0A, +0x0C, +0x0E; other bytes by kind | positions Angle12-style 16-bit wrap-around lerp (`lerpTurn(.., 16, t)`), angle Angle8, life/kind/colours Step | slot address + kind halfword (+0..+1); also life must not rise and the position must stay within a per-kind step (inferred; slots are reused by `0x80053570` with no generation, a same-kind reuse in one slot is otherwise indistinguishable; Open 4) | `0x800573C8` | `port: particles.h`, `draw_motion.cpp:ParticleMotion`, `native_particle_alloc.cpp:allocateParticleSlot`, ext `renderers.h Particle`, `loaders.c:762-766` |
| sparkles | `g_Sparkles` `0x80077108`, 8 x 0x18 | x, y, z s32 at +0, +4, +8; +0x0C fade; +0x0D full fade; +0x0E angle byte; +0x0F signed turn; +0x10 command word; +0x14 size and far depth | position Vector; angle Angle8; fade Scalar only while counting down; rest Step | slot address; alive while fade != 0 | `0x800584C4` (ages by `g_DeltaTime`, kills when outside retail's 4:3 view) | `port: glows.h`, `glow_producers.cpp:sparkleBetween` |
| glow emitters | `D_80078800` `0x80078800`, 16 x 0x24 | +0 rim count (0 = free); +4 rim table; +8 pointer to a position vector; +0x0C colour; +0x10 size; +0x14 offset x, y, z; +0x20 depth bias | pointee Vector (blend the 3 words at the pointer, usually a moby +0x0C or the flame anchor); size Scalar; rest Step | slot address + position pointer | `0x800580F4` | `port: glows.h`, `glow_producers.cpp:drawEmitters`, ext `graphics.h Glow`; Sparx's glow is `MobySparxProps.glow` (`moby.h:395`) |
| moby shadows | list of (record, size byte) pairs at `0x800724F4`, end in `0x80075F00` | see moby +0x1C | | moby slot | `0x80059F8C` | G:80059F8C |
| tracers | `g_TracerCount` `0x80075684`; `g_TracerPointCount[4]` `0x800772C8`; `g_TracerLists[4]` pointers at `0x80078658`; points 0x1C bytes: WorldPos 3 x s32, ScreenPos (draw-written), age +0x18 | | WorldPos Vector, age Step | list index + age (inferred: the head point is new each tick, so pair by age + delta) | `0x800189F0` (draws points with age <= 80; width ramps by index; level 63 width `>> 2`) | ext `draw.c:305-330`, G:800189F0; adders are overlay code, not reached by a run (`docs/issues/0196` pass 9), so the pairing is untested |

## Environment

| track | address / layout | kind | identity | read by | evidence |
|---|---|---|---|---|---|
| main sky | `g_Cyclorama` `0x80078A40` (sector count, pointer `0x80078A44`, occlusion count/groups `0x80078A48/4C`, background colour `0x80078A50`) | Skip (static level data; no time term found in `0x8004EBA8`, `0x8004F4BC`, `0x8004FEA0`, `0x80050240`); the drawn sky follows the camera matrices | | `0x8004EBA8(occlusion group, view, projection)` | G:8004EBA8 reads only `0x80078A40..4C`; ext `cyclorama.h` |
| background colour | `0x80078A50`, 3 bytes (copied into both draw envs' clear colour at section start, and used by the fog blend in `0x8001F798`/`0x80020F34`) | Scalar per channel (changes only in portal/level colour fades) | single | `0x8001ED5C`, 8001F798, 80020F34, 80050BD0 | G:8001ED5C:24-29 |
| portal records | pointers `g_Portals[6]` at `0x80078640`, count `0x800758BC`; Portal: +0 skybox, +4 point count, +8..+0x10 normal, +0x14 world sector, +0x18 path moby, +0x1C level id, +0x20 centre, points from +0x2C, 12 bytes each | static per level; centre and points Vector when `m_PathMoby` != 0 (inferred: a path portal follows its moby; no moving portal reached, Open 5) | record pointer | `0x80050BD0` | ext `cyclorama.h Portal`, G:80050BD0; `port: portals.h` |
| portal spin | `0x80075858` yaw (u16 mod 0x1000), `0x800758FC` pitch | see draw-side writes; Angle12 | single | `0x80050BD0` rotation | G:80050BD0 |
| portal pulse | `g_LevelTicks` `0x800758C8` (per field, from VSync) | Scalar (wrap 256 in the sine index `* 0x10`) | | `0x80050BD0` colour wave | G:80050BD0 (`_DAT_800758c8 * 0x10`) |
| portal colour | `D_800757D4` | Derived (a lerp from the portal's distance) | | | G:80050BD0 |
| terrain animation (platforms, torch-lit geometry, colours) | `g_EnvironmentAnimations` `0x80078560`: lists at `0x80078564` (texture swap), `0x8007856C` (scroll), `0x80078574` low poly, `0x8007857C` low colour, `0x80078584` high poly, `0x8007858C` high colour, `0x800785A4` collision; animation record: +1 flags, +2 key index, +3 ticks left | Step (keys apply at logic rate; marks at sector +0x18..+0x1B) | animation record address | classification pass of `0x800258F0` | G:8002A6FC, `port: guest_terrain_animation.h`; key fraction `key & 0xFF` feeds GTE IR0 (`fraction << 4`); blending inside a key needs a patch of the level's key table (Open 1) |
| texture swap | list `0x80078564`, 0x10-byte records at `0x800785C0`, 0xA8-byte at `0x800785C4` | Step (copies a texture record's UVs onto another) | record | terrain, sprite emitters | G:8002A6FC (first loop) |
| texture scroll | list `0x8007856C`; animation record +6 u16 position, wrap 0x80; UV bytes written to records: LQ `+1,+5,+9,+0xD = pos >> 2`; HQ `pos >> 1` with `+0x20`, `+0x10`, `+0x10`, `+0x10` phase offsets, masked 0x3F | Scalar on the position (wrap 128), UV bytes rebaked; or blend the baked UV bytes with wrap 32 (LQ) / 64 (HQ) (inferred) | animation record address | terrain (water, lava) | G:8002A6FC (second loop); the baked bytes are level data, so the replay must write them (Open 6) |
| skip-low-poly flag | `0x8007591C` | Step | | `0x8001F798`, `0x80020F34`, `0x80050BD0` | G:8001F798 |

## Screen and HUD

| track | address / layout | kind | read by | evidence |
|---|---|---|---|---|
| fade | `g_Fade` `0x80075918`, int 0..~32; update does `g_Fade -= g_DeltaTime` | Scalar (monotone, lerp) | `0x800190D4(2, g_Fade*8, ..)` | G:8001ED5C:41-44, ext `update.c:1144-1150`; a rising fade (set to 15/32 by init) is a Step |
| border | `0x800756C0` height, `0x8007570C` enable | height Scalar via the draw-side rule above; enable Step | `0x80018F30` | ext `draw.c:429-441` |
| HUD mobys | `g_Hud` `0x80077FA8` (ext `hud.h`): display states +0..+4, progress +5..+9, steady ticks +0x0C..+0x1C, counts +0x20..+0x30, `unk_0x3C` +0x3C (spin counter), `unk_0x40` +0x40 (egg frame), `m_Mobys[12]` +0x44 (0x58 each) | slide: moby +0x10 (y) Vector (`target + g_HudOpeningOffsets[progress]`, 13-step table, one step per update); spin: moby +0x46 (z byte) Angle8, `m_Mobys[4]` y/z from `unk_0x3C`; digit class (+0x36), substate (+0x49), render radius (+0x50) Step | queue list of `0x80022A2C` via `0x80019300` | ext `hud.c:44-74,155-200`, `draw.c:528-575`; slides and spins can lerp by slot index; a class, state or display-state change is a Step |
| HUD sprites | `m_SpriteRect[32]` at `g_Hud + 0x464`, 8 bytes (x, y, w, h shorts): eggs 0..11, life orbs 12..31 | y Vector (egg slide); orb x, y Vector; orb colour Scalar (see below); egg texture `(unk_0x40 + i) % 9 + 1` Step | `0x8001919C` | ext `hud.c:52-57`, `draw.c:577-601` |
| orb colour | `D_800770C8.m_specularTime` `0x800770F4`, wraps 256 | Angle8-style wrap 256 | `0x80019300` (`COSINE_8((time - i*256/20) & 0xFF) >> 7) + 128`) | ext `draw.c:577-585`, `specular_and_metal.c` |
| gem counter text | `g_Hud.m_GemSteadyTicks`, `g_Hud.m_GemCount` | Step (the text mobys are rebuilt per draw; their wave is `COSINE_8(ticks*4 + i*12)`) | `0x80019300` | ext `draw.c:550-562` |
| demo text | `g_LevelTicks` | Step | `0x80018908` | ext `draw.c:276-279` |

Should the HUD be Step? The pieces that move are one-step-per-update slides and spins; lerping them per slot
is safe while the display state and class are unchanged (the OpenPete trouble with gems and eggs is the
digit and egg-frame swaps, which are Step). The counters, digits and egg frames are Step.

## Cuts

A cut keeps the newer value for every track (no blend) in the frame it appears.

| marker | address | evidence |
|---|---|---|
| draw skipped on a state switch: `g_StateSwitch` set to 1 by loads, respawn, level exits, fairy, cutscene starts; cleared at the start of `GamestateUpdate` | `0x8007579C` (writers `0x8001364C`, `0x8002C664`, `0x8002C924`, `0x8002CCC8`, `0x8002D440`, `0x8002D580`, `0x8002E12C`, `0x80032B08`, `0x800334D4`, `0x8003385C`) | ext `main.c:34`, `update.c:1040`; refs listed above |
| game state change | `g_Gamestate` `0x800757D8` (0 = playing) | G:8001ED5C:23, ext `common.h` |
| load stage | `g_LoadStage` `0x80075864` (>= 0 while a level loads), `g_LevelId` `0x8007596C`, `g_NextLevelId` `0x800758B4` | ext `camera.c:526-532`, `port: guest_globals.h` |
| camera forced to destination | `g_Camera.unk_0xCC` = `0x80076E9C` set to 1 and `D_800756B8` (`0x800756B8`) set to 1 by `CameraForceToDestination` `0x80034C84`, which copies the destination into the position and zeroes the smoothing counters; callers `0x80035FB4` (fires when the sum of absolute spherical deltas at `g_Camera + 0xA8..0xBC` is at least 0x21) and `0x80037A20` (level-transition camera) | ext `camera.c:230-236`; Ghidra callers at `0x80036A08`, `0x80037B40`; asm `func_80035FB4.s` around `0x800369A4` (`slti 0x21`). `D_800756B8` is cleared in `0x80034480` (`0x80034784`) |
| camera reset | `0x80034358` (copies the sphere state, positions the camera at the focus, zeroes screen shake); callers on level start, fairy exit, balloonist exit | ext `camera.c:178-216`, `loaders.c:515`, `init.c:351,529` |
| camera state | `g_Camera.m_State` `0x80076E28` (+0x58; `0x8000000E` intro fly-in, `0x80000012` level transition, `0x8000000B`) changes the update path; the state value is not read by the draw | ext `camera.c:546-575`; address by struct offset, not queried |
| Spyro reset | `0x8004AC24(keepPosition)`: clears the record (0x2A4 bytes) when keeping position, sets progress/frame/sorting depth (`0x80078A7F = 4`, `0x80078A77 = 1`); with 0 it zeroes the tint word and velocities. Callers at level load, respawn, fairy/balloonist exits | G:8004AC24; ext `init.c:129,330,410,506,614`, `update.c:667,783`, `loaders.c:456` |
| Spyro hidden | `0x80075814` | G:80019698 |
| moby slot freed/reused | state byte +0x48 of a dynamic slot goes 0xFE/0xFD/0xFF and back; level mobys killed go bit 7 set | `port: native_moby_allocator.cpp` (`allocateDynamicMoby`, `releaseMoby`) |
| particle slot reuse | kind byte (+1) 0xFE released, 0xFF free, new kind stamped by `0x80053570` | `port: native_particle_alloc.cpp` |
| warps / teleports | a title warp or `mobys` debug teleport moves records between logic frames with no marker beyond the above; use a per-track move limit as the backstop | `port: core/debug/teleport.cpp`, `warp.cpp` |
| respawn spiral, game over | `GS_Respawn`/`GS_GameOver` draw `0x8001CA38`, not the playing section | G:8001ED5C:74 |

## Open

1. Terrain animation is applied by the draw into level data per key; its in-key fraction (key byte 0 at
   `def + 0x0C + key*8 + 4`) cannot be blended without patching the key table. Replays show each new key
   from `t = 0`. Not measured against a moving platform.
2. Angle8 quantisation: moby and Spyro angle bytes, and the title-letter case (`0196` pass 9), lose the
   sub-byte phase the guest's table cannot express. Body angles have a 12-bit source (`0x80078B74..7C`);
   mobys and head/tail angles have none found. A replay writes rounded bytes unless the sine lookup is
   patched; the replay writes the stepped bytes and notes the sixteenths past them (`replay_angles.h`), which the list builders compose.
3. Moby identity in the dynamic pool has no generation word; the cut limit per class is undecided.
4. Particle identity is slot + kind only; the life-monotone and step-limit rules are inferred, and same-kind
   particles in neighbouring slots can still mispair (the OpenPete streaking).
5. Whether any Spyro 1 level moves portal records (`m_PathMoby`); none reached.
6. Texture scroll: whether the replay should blend the position (+6) and rebake, or blend baked UV bytes. The
   rebake lives inside `0x8002A6FC`, which the replay does not call.
7. Addresses not resolved: `MetalBuildMatrix`, `SpecularUpdate`, `HudReset`, `HudTick` call order inside
   `GamestateUpdate` (the Ghidra image memory ends at `0x800757FF`, so `--refs` answers "outside memory" for
   `0x800758C8`, `0x80075858`, `0x800770E0`, `0x80078640`, `0x8007AA20`, `0x8007591C`, `0x80076E9C`; only
   their uses in decompiled C were read). `HudTick` is `0x80054988` (G: 913 insns) by its HUD stores.
8. Whether gameplay calls `SpecularUpdate` (the light direction `0x800770C8` rotates with
   `m_specularTime` on menus per ext `specular_and_metal.c`); the moby light `0x800770D4` and queue light
   `0x800770C8` are read by `0x80020F34` and `0x80022A2C`.
9. Tracers (levels 50, 63) and their adders were not reached; pairing by age is untested.
10. Replay `g_DeltaTime` (`0x800756CC`): sparkles and the border age by it inside the section. Either keep
    the real delta and pre-offset the aged inputs (rule above) or set it to 0 and provide blended aged
    records, as the producers do. Not decided.
11. `0x800522C0`'s speed-divider argument (`param_2`) and its effect on moby progress per logic frame not
    traced.
