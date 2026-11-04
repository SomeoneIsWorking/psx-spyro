#include "render/field/moby_pose.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_word.h"

#include <array>

namespace spyro::moby_pose {
namespace {

using guest_word::asSigned;
using guest_word::asWord;
using guest_word::packedFields;
using guest_word::signedHalf;
using guest_word::sra;

// Mesh header words the drawer's vertex loop reads.
constexpr std::uint32_t kMeshBaseDeltas = 0x0C; // one word per vertex, every frame
constexpr std::uint32_t kMeshShortStreams = 0x20;

// The drawer's three running coordinates, in the order it loads IR1..IR3: z, x, y.
using Running = std::array<std::int32_t, 3>;

// SCUS_944.25 80044714..80044890: one frame's streams, vertex by vertex.
std::vector<Running> decodeFrame(Core &core,
                                 std::uint32_t deltaTable,
                                 std::uint32_t mesh,
                                 std::uint32_t streams,
                                 std::uint32_t count) {
  std::uint32_t halves = (streams & 0xFFFFu) * 2 + core.mem_r32(mesh + kMeshShortStreams);
  std::uint32_t bytes = ((streams >> 15) & 0x3FEu) + halves;
  std::uint32_t fromHalves = (streams >> 25) & 1u;
  std::uint32_t base = core.mem_r32(mesh + kMeshBaseDeltas);
  Running at{};
  std::vector<Running> frame;
  frame.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i, base += 4) {
    if (fromHalves == 0) {
      const std::uint32_t code = core.mem_r8(bytes++);
      fromHalves = code & 1u;
      const auto delta =
          packedFields(core.mem_r32(deltaTable + (code & 0xFEu) * 2) + core.mem_r32(base));
      at[0] += asSigned(delta[0]);
      at[1] -= asSigned(delta[1]);
      at[2] -= asSigned(delta[2]);
    } else {
      const std::uint32_t code = signedHalf(core.mem_r16(halves));
      halves += 2;
      fromHalves = code & 1u;
      if ((code & 2u) != 0) {
        const std::uint32_t second = signedHalf(core.mem_r16(halves));
        halves += 2;
        at[0] = asSigned(sra(second, 5));
        at[1] = asSigned(sra(second << 26, 21) | ((code & 0xF000u) >> 11));
        at[2] = asSigned(sra((code & 0xFFCu) << 20, 20));
      } else {
        const auto delta = packedFields(core.mem_r32(base));
        at[0] += asSigned(sra(code, 9)) + asSigned(delta[0]);
        at[1] -= asSigned(sra(code << 21, 25)) + asSigned(delta[1]);
        at[2] -= asSigned(sra(code << 26, 27)) * 4 + asSigned(delta[2]);
      }
    }
    frame.push_back(at);
  }
  return frame;
}

Vertex vertexOf(const Running &at) {
  return {.x = at[1], .y = at[2], .z = at[0]};
}

} // namespace

std::vector<Vertex>
entryPose(Core &core, std::uint32_t deltaTable, std::uint32_t entry, std::uint32_t count) {
  namespace e = guest_moby::entry;
  const std::uint32_t classWord = core.mem_r32(entry + e::kClass);
  const std::vector<Running> frameA = decodeFrame(
      core, deltaTable, core.mem_r32(entry + e::kMesh), core.mem_r32(entry + e::kTexture), count);
  std::vector<Vertex> pose;
  pose.reserve(count);
  // 80044588: frame B's share, 0 drawing frame A alone.
  const std::uint32_t share = (classWord >> 4) & 0xFF0u;
  if (share == 0) {
    for (const Running &at : frameA) {
      pose.push_back(vertexOf(at));
    }
    return pose;
  }
  const std::vector<Running> frameB = decodeFrame(core,
                                                  deltaTable,
                                                  core.mem_r32(entry + e::kOverlayMesh),
                                                  core.mem_r32(entry + e::kOverlayTexture),
                                                  count);
  psx::cpu::Gte &gte = core.gte();
  const GteRawState saved = gte.saveRaw();
  gte.writeData(psx::gte::kIr0, share);
  for (std::uint32_t i = 0; i < count; ++i) {
    gte.writeData(psx::gte::kIr1, asWord(frameA[i][0]));
    gte.writeData(psx::gte::kIr2, asWord(frameA[i][1]));
    gte.writeData(psx::gte::kIr3, asWord(frameA[i][2]));
    gte.writeControl(psx::gte::kFarRed, asWord(frameB[i][0]));
    gte.writeControl(psx::gte::kFarGreen, asWord(frameB[i][1]));
    gte.writeControl(psx::gte::kFarBlue, asWord(frameB[i][2]));
    gte.execute(psx::gte::kIntpl);
    pose.push_back(vertexOf({asSigned(gte.readData(psx::gte::kMac1)),
                             asSigned(gte.readData(psx::gte::kMac2)),
                             asSigned(gte.readData(psx::gte::kMac3))}));
  }
  gte.restoreRaw(saved);
  return pose;
}

} // namespace spyro::moby_pose
