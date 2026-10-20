#include "spyro1/core/audio/spu_library.h"

namespace spyro1::native {

void registerSpuLibraryOverrides(Core &core) {
  registerAudioKeyStateOverrides(core);
  registerSpuRegisterOverrides(core);
  registerSpuVoicePitchOverrides(core);
  registerSpuStateOverrides(core);
  registerSpuCallbackOverrides(core);
  registerSpuVoiceAttributeOverrides(core);
  registerSpuKeyStatusOverrides(core);
  registerSpuTransferModeOverrides(core);
  registerSpuCommonAttrOverrides(core);
}

} // namespace spyro1::native
