# 0256: Spyro 1's skip press reaches the next state; the intro and the dragon speech refuse Start

Status: resolved (Spyro 1). Owner: `titles/spyro1/core/frame/transition_skip.cpp`.

## Observed
USER, 2026-11-17: "if you skip the screen right before the game starts, the start button carries over and
opens the pause menu, it shouldn't." and "First ever dragon cutscene is not skippable".

## Reproduced
`scratch/startcarry/repro.py` (New Game from the save picker, hidden window, `ires=0`, 3840x2160 sink,
unpaced), on the binary before the fix (`before.out`):

- Start tapped (4 fields) on the intro cutscene ("IN THE WORLD OF DRAGONS", GS_Cutscene, `g_CutsceneIdx` 1) at
  layout tick 60: `m_Duration` stays 1472 and the cutscene plays on for 1420 more steps.
- Start tapped (6 fields) on "THE ADVENTURE BEGINS..." once its load reached stage 13: the flyby ends into
  GS_Playing (2 steps later), the next field shows `g_Pad.m_Down = 0x0800` and the following update is
  GS_PauseMenu.

## Cause
- Carry-over: `titles/spyro1/core/frame/transition_skip.cpp:TransitionSkip::observe` acted on
  `FieldOwner`'s host pad edge, which leads the guest's `g_Pad` by a field or more. The flyby's terminal
  `LoadLevel(1)` put the game in GS_Playing before the guest had seen the press; the press then reached
  `g_Pad.m_Down` as an ordinary edge and GS_Playing's update opened the pause menu on it (step 53: `m_Down`
  0x0800, step 54: GS_PauseMenu). The terminal's `LoadLevelScene` also runs `PadReset(&g_Pad)`, which clears
  `m_Held`, so a Start the guest had seen and still held would come back as a new edge too.
- Intro: `GamestateCutsceneUpdate` takes held Start/Cross only from tick 241 (about 4 s in); before that it
  refuses the press, and the port had no arm for that range.
- Dragon: GS_Dragon's speech (`func_8002F3E4` m_State 4) reads only Cross, and only when the rescued
  dragon's props word 13 is zero; Start does nothing there. Artisans' first dragon (Nestor, `warp 0/0/2`)
  has props word 13 = 1, so retail refuses even Cross: Start at speech tick 30 left the speech running
  512 more steps (`dragon_before.out`).

## Fix
`TransitionSkip` reads its press where the guest's own update does, Start or Cross in `g_Pad.m_Down` at
the start of the update (none while a recorded demo drives the pad), and treats a press that cancels as
the screen's: after the terminal it takes Start and Cross out of `g_Pad.m_Down` and the four buffered
`m_Down` words, and puts back into `m_Held` the skip buttons held before the terminal, so a button still
held makes no new edge after the guest's `PadReset`. The press no longer comes from `FieldOwner`.
Two arms are added:

- `IntroCutscene`: before tick 241, the guest's own write, `m_Duration = (tick >> 1) + 16` on the tick the
  update will see, so the cutscene fades out into `EndCutscenePlayback`. From 241 the guest's held test
  stays the owner.
- `DragonSpeech`: during m_State 4, `m_CutsceneTicks` is brought to twice the camera track length, so the
  state's own test takes its terminal branch at `0x80030D54` (the voice stopped, the camera on the track's
  last frame), the branch its Cross skip and its natural end share.

Spyro 2 and 3 are not affected, by construction rather than by a run: their counted skips
(`game/core/counted_screen_skip.cpp`) keep at least two updates of the screen after the press, so the
press reaches the screen's own updates before gameplay, and their arrival flyby ends on the guest's own
Start. "First ever dragon cutscene" is Spyro 1's.

## Shown in the product
Clang `build/agent`, hidden window, unpaced, `ires=0`, 3840x2160 sink, `scratch/startcarry/`:

- `repro.py after2 60 4`: Start at intro tick 60 logs `intro cutscene fades out (1) from tick 64; duration
  1472 -> 48`; the cutscene fades out and the flyby starts 18 steps later (1420 before). Start on the flyby
  at load stage 13 logs `level flyby cancelled (2)`, GS_Playing at the next step, `m_Held` 0x0800 kept over
  the `PadReset`, no Start edge, and the game stays in GS_Playing for the 16 steps sampled after.
- `dragon.py dragon_after2 ... 30 0/0/2`: Start at Nestor's speech tick 30 logs `dragon speech ended (1) at
  tick 32 of 1054`; m_State 5 two steps later instead of 512. State 5's entry camera (85474, 49506, 11063)
  and Spyro (84992, 49935, 9252), and gameplay+30's Spyro (84992, 49887, 9259) and camera
  (85766, 47939, 10711), equal the unpressed run's (`dragon_natural.out`).
- `sheet.png`: the intro fading from the press; the flyby, the gameplay fade-in and gameplay with no pause
  menu; Nestor's speech, the shrink and gameplay.
