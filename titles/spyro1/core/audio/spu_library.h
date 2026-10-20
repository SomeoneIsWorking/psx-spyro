// Spyro 1's PsyQ libspu (external/spyro-1 asm/psyq.s): its data words and the native entries.
#pragma once

#include <cstdint>

class Core;

namespace spyro1::libspu {

// Initialised to 0x1F801C00, voice 0's left volume; indexed as the SPU register file.
inline constexpr std::uint32_t kSpuRegisterBase = 0x80073554u;
// The 5-bit scale between a guest quantity and its register field.
inline constexpr std::uint32_t kSpuFieldShift = 0x8007357Cu;
// The keys the SPU holds down.
inline constexpr std::uint32_t kSpuKeyStatus = 0x800730ECu;
// The library's pointer to its own state structure.
inline constexpr std::uint32_t kSpuStatePointer = 0x80073568u;
// SpuSetVoicePitch's rounding.
inline constexpr std::uint32_t kSpuPitchRoundingGuard = 0x80073578u;
inline constexpr std::uint32_t kSpuPitchRoundingStep = 0x80073580u;
inline constexpr std::uint32_t kSpuPitchRoundingMask = 0x80073584u;

} // namespace spyro1::libspu

namespace spyro1::native {

// Every libspu entry Spyro 1 owns natively.
void registerSpuLibraryOverrides(Core &core);

void registerAudioKeyStateOverrides(Core &core);
void registerSpuCallbackOverrides(Core &core);
void registerSpuCommonAttrOverrides(Core &core);
void registerSpuKeyStatusOverrides(Core &core);
void registerSpuRegisterOverrides(Core &core);
void registerSpuStateOverrides(Core &core);
void registerSpuTransferModeOverrides(Core &core);
void registerSpuVoiceAttributeOverrides(Core &core);
void registerSpuVoicePitchOverrides(Core &core);

} // namespace spyro1::native
