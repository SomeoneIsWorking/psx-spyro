# 0252 — Right-stick camera (G007)

User, 2026-11-13: "Oh right, Spyro gets free camera right stick".

## Prior art

- OpenPete (Spyro 1) has a keyboard free-fly camera; it is closed source, so it does not exist for us.
- Spyro Scope (FranklyGD, GPL-3.0) rotates the view with the mouse and teleports Spyro to the camera:
  evidence only, for the camera addresses it reads in each title.
- No PS1 patch or GameShark code gives Spyro 1/2/3 a right-stick camera; Reignited's is a remake.

## Design

- RE each title's gameplay camera update: the state L2/R2 rotation and the Triangle look mode write
  (Spyro 1: `g_Camera` focus rotation and spherical preset in the open-spyro decomp; Spyro 2/3: the
  matching fields in the spyro-2/spyro-3 decomps). Names go into the decomp forks first.
- One shared owner in `game/` reads the right stick from the pad owner each logic tick and offsets
  the camera's target azimuth and elevation the way the L2/R2 path does, clamped to the game's own
  limits. Each title supplies its field addresses from its decomp's symbol map.
- Applied only in the gameplay states whose camera follows Spyro; cutscenes, flybys, the balloonist,
  portals and menus are left alone.
- The offset is camera input, so the existing motion tracks interpolate it and the game's own
  collision and cuts apply.

## Spyro 1 (done)

- The pad owner reads the right stick (psxport `Pad::rightStick()`, centred during a pad replay or while the
  host holds the input); `game/core/stick_camera.*` turns it into a deflection past the rest deadzone and
  owns the `right_stick_camera` setting row (on by default).
- Yaw: `UpdateCameraManualYawInput` (0x80035F58, `titles/spyro1/core/camera/camera_input.cpp`) writes
  `g_nCameraManualYawInput` (0x80075924) from L2/R2 as +-0x400; with neither held the stick's x writes the
  same word, full deflection matching a shoulder. `AdvanceCameraSpringStep` springs the yaw on it, so the
  game's own refusals (lock-on, stuck camera) and its mode list (no free look, scripted target or
  vantage, level transition, portal path, charge) apply.
- Pitch: after `LoadCameraTargetAnglesFromMode` (0x80034198) loads the target, the stick's y moves
  `g_nCameraTargetPitch` in the same modes, with the free look's own limits (up 0x500, down 0x3D0, past
  0x140 down into the pitch bias) and no higher than the overhead fall preset (pitch 0x400). Not
  inverted: stick up looks up.
- Proof: Stone Hill, `ires=0`, `fps60=1`, sink 3840x2160, unpaced: a full-right stick orbits the camera
  around Spyro with the skyline moving 8-9.5 native px on every present (in-betweens halfway); stick up
  looks into the sky, down looks down from overhead (`scratch/stick-camera/s1_orbit_sheet.png`,
  `s1_orbit_crops.png`).
- The `.pad` recording does not carry the stick, so a recording that used it does not replay its camera.

## Spyro 2 and 3

- One owner, `game/core/follow_camera.*`, with each title's addresses in its runtime (spyro-2 `src/camera.c`,
  spyro-3 asm). Both poll the shoulders in camera modes 0, 1 and 6 (S2 `func_8001F020` R1/R2 and L1/L2, S3
  `func_80013918` L2/R2, refused in gamestate 7 and with L1/R1 held, both refused while `g_Spyro+0x240` is set)
  into the azimuth override at camera+0x150 with the rotating state at +0x154; the stick's x writes the same
  override, full deflection matching a shoulder, only when no shoulder is held.
- Pitch: the error computation (S2 `func_8001DB58`, S3 `func_8001241C`), called by the camera's own error step
  and by a level's error callback (S2 `D_80067040`, S3 `0x80074308`; Glimmer's is just the computation and the
  step), runs with the stick's look added to the target row's elevation (camera+0x94) and the free look's 0x140
  split on its yaw word (+0x9C), and the game's target restored after it. Same limits as Spyro 1; only in the
  poll's modes, unlocked (+0x134 zero) and unrefused.
- Proof: Glimmer (Spyro 2) and Sunny Villa (Spyro 3), each `warp 0/1/0`, at `ires=0`, `fps60=1`, 3840x2160, unpaced: stick right orbits the
  camera every present, stick up looks into the sky, down looks down on Spyro from overhead
  (`scratch/stick-camera/s2_sheet.png`, `s2_pitch_sheet.png`, `s3_sheet.png`).

## Phases

1. Spyro 1: RE the camera update, the owner, and proof by crops of an orbit at 60 fps (done).
2. Spyro 2 and 3 on the same owner (done).
3. Setting row and docs.
