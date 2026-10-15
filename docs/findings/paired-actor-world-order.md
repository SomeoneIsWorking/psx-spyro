# Paired actor placement in the FIELD ordering table

Spyro 1's paired actor renderer `0x80023AC4` coalesces its model's local face buckets into a much
smaller global depth range before appending them to the world ordering table. A port that keys the
local bucket straight into the world table draws the actor behind foreground terrain. The native
renderer that made this mistake was deleted with the native path (2026-10-15); this contract is the
input for the paired-actor producer (issue 0186).

## Binary contract

The full renderer is `0x80023AC4..0x800258EF`, including the tail beyond the short function extent in
some symbol metadata. The retained address/raw-byte listing in
`external/spyro-1/asm/renderers/r_pete.s` was checked with the project's locked Python environment
against RAM obtained from the independent full-console oracle: **1,931 instruction words scanned,
1,931 equal, zero mismatches**. Product and oracle RAM also contained identical bytes across this
range. The listing is corroborating RE material, never a generated product input.

- `0x80023F8C..0x80023FD0` sets `depthNear = max(0, (baseMACz >> 7) - instance[39])` and
  `depthOrigin = max(0, baseMACz - (512 << control))`, with signed 32-bit wrapping before each clamp.
  `control` is the maximum active model byte 11. CR13/14/15 hold control, depthNear, and depthOrigin.
- `0x80024C38..0x80024C58` establishes local pair base `0x8006FCF4`, a 288-pair span, and local
  shift `control + 4`. Paired local buckets use this fixed base.
- `0x800257AC..0x80025800` computes the initial global byte offset from the greatest occupied pair:
  `(depthNear << 3) + (32 << control) - 8 - adjustment`, then clamps negative offsets to zero.
  With `maxPair` expressed in bytes relative to the fixed local base,
  `adjustment = ((max(0, 2048 - maxPair) << control) >> 8) << 3`, preserving unsigned shift wrapping.
- `0x80025818..0x80025890` scans descending local pairs in chunks of `256 >> control` bytes.
  Unoccupied buckets consume their place. Each chunk appends local FIFO chains to its global slot;
  the next chunk goes to the preceding global slot. The modeled normal path requires chunks of at
  least one eight-byte pair.
- `0x80025894..0x800258A0` clamps at global slot zero. The branch delay slot first subtracts eight;
  the zero case adds that eight back. Clamping zero to slot one instead is wrong.

The renderer does not write OFX, OFY, or H anywhere in this complete range. A producer must use its own
projection parameters rather than assume the renderer publishes the GTE controls.
