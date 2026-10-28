// pause_backdrop.h — the pause backdrop of func_8001A40C (gamestates 2, 3 and 6, read again by gamestate 10's
// func_8001C694). On its first frame the guest reads the displayed picture back in four 128-wide strips, turns
// each into 5-bit grey indices with func_80017E98 and loads them as an 8-bit page at VRAM (512, 0); later frames
// tint four FT4s over x 0..512 with that page. The conversion's override declares the page a read-back texture,
// so the record shows the widened picture, margins included, through the guest's own quads.
#pragma once

class Core;

namespace spyro1::pause_backdrop {

void registerOverrides(Core &core);

} // namespace spyro1::pause_backdrop
