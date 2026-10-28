// The pause backdrop's grey page at VRAM (512, 0) (func_8001A40C), declared a read-back texture.
#pragma once

class Core;

namespace spyro1::pause_backdrop {

void registerOverrides(Core &core);

} // namespace spyro1::pause_backdrop
