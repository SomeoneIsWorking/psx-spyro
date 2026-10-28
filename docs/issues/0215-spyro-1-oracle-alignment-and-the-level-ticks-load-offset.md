# 0215 — Spyro 1 oracle alignment, and the level-tick offset every load leaves

Status: open (2026-10-28). Owner of the remaining divergence: the level load's last draw-less iteration
(`titles/spyro1/core/frame/frame_driver.cpp`, `FrameDriver::stepFrame`); see 0110.

## Alignment fixed in the comparison
- `tools/oracle_spyro1.py` `advance`: the product's step is one logic frame after the boot prefix (whose steps run
  no guest frame, so it steps until `g_DeltaTime` is first written); `compare.step_until_counter_consumed` overshot
  when a load left the counter at 0 and is deleted. `playing` matches.
- `lookahead` is 0 on the console and 1 on the product: the console parks right after the update, the product
  after the update and its fields. The pad words show the next poll on the product, so they are informational.
- The SPU reverb configuration is decisive only while reverb is heard (`psxport/tools/oracle/spu_state.py`): the
  console's BIOS shell leaves one, Spyro never writes or enables it.
- With every gameplay and SPU override shadowed (`PSXPORT_OVERRIDE_DIFF`) the route showed no mismatch.

## Product defects found and fixed
- psxport 0187: the level sample bank was cut at 64K words.
- psxport 0188: the field rate; the intro narration outlived its cutscene and left its sound slot at 0.

## Remaining divergences (`scratch/player-bugs-4/probe/full_route3.log`)
1. **Load offset, `playing` to gameplay[3] frame 25.** `g_LevelTicks` is one tick behind on the product (2 against 3
   at `playing`), with every decisive range equal. The dragon cutscene state at 0x80077058 counts from it, so it
   starts at gameplay[2] frame 9 on the product and frame 10 on the console. Cause not traced past 0110.
2. **The console's CD wait, gameplay[3] frame 26 onward.** At that iteration the console ran a 6-field main-loop
   window (`g_DeltaTime` 4 against the product's 2): a synchronous `CDLoadSync` read with `g_CDMaxReadTime` 600, measured
   in 0110 ("RESOLVED 2026-09-28"). The product's CD is instant by design, so it spends no fields there. The offset
   grows to 5, then 6 at frame 28 (a 3-field console window), and stays 6 to route end (0x2E against 0x34). `player`
   differs from that frame (3, then 9 of 680 bytes) and `camera` from gameplay[3] frame 39 (1 to 35 of 272 bytes, the
   rotation 0110 traced to `g_DeltaTime`), both through the end of the route. Both ranges are informational, which is the
   only reason the route reads "all decisive RAM equal"; `camera` is picture-decisive, so no picture past frame 26
   of gameplay[3] compares. This is the load removal the port keeps, not a defect: the product's world advances
   with the fields it presents. A comparison across it must align on the console's CD wait, not on frames.
3. **SPU rows.** The gameplay[3]/[4] voice rows follow from items 1 and 2 (the case 2 voice swap,
   `StopSoundVoiceByIndex` then sound 0x31). The single-frame rows in gameplay[1] (frames 10, 29, 31, 55) are a voice
   ending one frame apart on the two cores; their cause is not traced.
