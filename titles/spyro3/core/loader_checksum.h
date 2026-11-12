#pragma once

class Core;

namespace spyro3 {

// The loading screen's loader 0x8002CA50 checksums code only while its read poll 0x800503F8 reports
// a read in flight and its stage 0x8006C518 is 2..7, 0x1400 steps a call; stage 7 waits for the
// checksum's end and stage 8 sabotages 0x8002AB38 unless the sum is 0 (issue 0247). Its reads are
// due at once, so no read is ever in flight: before the loader runs, the override completes the
// checksum those calls would have run, from the same bytes, then runs the loader.
void installLoaderChecksum(Core &core);

} // namespace spyro3
