// guest_packet.h — the GP0 packet tags, sizes and command words the Spyro emitters write.
#pragma once

#include "gp0_command.h"

#include <cstdint>

namespace spyro::guest_packet {

// Tags carry the word count in the top byte, the link left clear.
inline constexpr std::uint32_t kGt4Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyGt4);
inline constexpr std::uint32_t kGt3Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyGt3);
inline constexpr std::uint32_t kG4Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyG4);
inline constexpr std::uint32_t kG3Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyG3);
inline constexpr std::uint32_t kFt4Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyFt4);
inline constexpr std::uint32_t kFt3Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyFt3);
inline constexpr std::uint32_t kF4Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyF4);
inline constexpr std::uint32_t kF3Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kPolyF3);
inline constexpr std::uint32_t kLineF2Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kLineF2);
inline constexpr std::uint32_t kLineG2Tag = psx::gpu::Gp0Command::packetTag(psx::gpu::kLineG2);

// Packet bytes, tag word included.
inline constexpr std::uint32_t kGt4Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyGt4);
inline constexpr std::uint32_t kGt3Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyGt3);
inline constexpr std::uint32_t kG4Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyG4);
inline constexpr std::uint32_t kG3Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyG3);
inline constexpr std::uint32_t kF4Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyF4);
inline constexpr std::uint32_t kFt3Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyFt3);
inline constexpr std::uint32_t kF3Bytes = psx::gpu::Gp0Command::packetBytes(psx::gpu::kPolyF3);

// Command words: the opcode in the top byte, a colour ORed or added below it.
inline constexpr std::uint32_t kSemiCode = psx::gpu::Gp0Command::opcodeWord(psx::gpu::Gp0Command::kFlagSemiTransparent);
inline constexpr std::uint32_t kGt4Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyGt4);
inline constexpr std::uint32_t kGt3Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyGt3);
inline constexpr std::uint32_t kSemiGt4Code = kGt4Code | kSemiCode;
inline constexpr std::uint32_t kSemiGt3Code = kGt3Code | kSemiCode;
inline constexpr std::uint32_t kG4Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyG4);
inline constexpr std::uint32_t kG3Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyG3);
inline constexpr std::uint32_t kFt4Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyFt4);
inline constexpr std::uint32_t kSemiFt3Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyFt3) | kSemiCode;
inline constexpr std::uint32_t kF4Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyF4);
inline constexpr std::uint32_t kF3Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kPolyF3);
inline constexpr std::uint32_t kLineF2Code = psx::gpu::Gp0Command::opcodeWord(psx::gpu::kLineF2);

} // namespace spyro::guest_packet
