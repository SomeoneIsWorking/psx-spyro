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

## Phases

1. Spyro 1: RE the camera update, the owner, and proof by crops of an orbit at 60 fps.
2. Spyro 2 and 3 on the same owner.
3. Setting row and docs.
