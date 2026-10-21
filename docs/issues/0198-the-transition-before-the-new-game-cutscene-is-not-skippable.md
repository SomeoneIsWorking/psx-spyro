# 0198: the transition before the new-game cutscene is not skippable

Status: resolved (Spyro 1)

## Observed
USER, 2026-10-20: "only one transition scene is skippable (after the new game cutscene, the one BEFORE
the cutscene is still not skippable)".

Start skips the screen after the new-game cutscene (`TransitionSkip`, issue 0129). The transition from
New Game into the cutscene ignores Start.

## Reproduced
Spyro 1, New Game from the save picker (driven by `title_prompts.new_game_prompt`), Start tapped once on the
"IN THE WORLD OF DRAGONS..." card at `m_Tick` 60 with `g_LoadStage` 7: the card ran on to `m_Tick` 384 and
entered `GS_Cutscene` 325 frames after the press (`scratch/0198/before/`).

## Cause
`titles/spyro1/core/frame/transition_skip.cpp:classify` only authorised the `TSD_Level` flyby. The card before
the intro is the `TSD_Cutscene` arm of `GamestateCutsceneTransition` (`0x80032B08`), and its terminal is inline
(`0x8003304C..0x800330E4`, `docs/findings/start-skip-map.md`), ending in that function's epilogue, so it was
left out as undispatchable.

## Fix
`TransitionSkip` has a `CutsceneTransitionIntroCard` arm: on Start or Cross while the card is up and
`g_LoadStage == 7` (an earlier press is held), it lowers `$sp` by the function's `0x38` frame, spills `$ra`
(the frame update's call site return, `0x800339F4`) and `$s0..$s2` where the prologue does, and runs the
guest from `0x8003304C` through its own epilogue. `m_Tick` is not written.

## Shown in the product
Clang `build/agent`, hidden window (`PSXPORT_VK_HEADLESS=1` in the process environment),
`scratch/0198/repro.py after 150`: Start at `m_Tick` 150 logs `intro card cancelled (1); guest terminal
0x8003304C left stage 14 load stage 10`, and `GS_Cutscene` starts 2 frames after the press. The first
cutscene frame (`scratch/0198/after/2-cutscene-start.png`) matches the unskipped one
(`scratch/0198/before/2-cutscene-start.png`); the cutscene plays (`3-cutscene-240.png`), its held Start ends it, the
"THE ADVENTURE BEGINS..." flyby follows, and Artisans (level 10) is in `GS_Playing` (`4-playing.png`).

## Spyro 2 and 3
Neither has a transition-skip owner yet (S034, S035 missing). Spyro 2's executable has no flyby card text;
Spyro 3's carries "THE ADVENTURE BEGINS", "THE ADVENTURE CONTINUES" and "A Desperate Rescue Begins...", so it
probably has an equivalent card. Not reproduced here; it belongs to S035's skip work.
