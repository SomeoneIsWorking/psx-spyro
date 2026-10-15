// record_frames.h — a chain walked by DrawOTag into the device's record, and the comparison of
// record primitives the Record tests share.
#pragma once

#include "core.h"
#include "frame_record.h"
#include "gpu_ports.h"

#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

namespace spyro::test {

// DrawOTag: the GPU's record of the chain at `head`, sent by DMA2 in linked-list mode.
inline psx::present::FrameRecord submitChain(Core &core, std::uint32_t head) {
  constexpr std::uint32_t kGp0 = 0x1F801810u;
  constexpr std::uint32_t kGp1 = 0x1F801814u;
  constexpr std::uint32_t kDma2Address = 0x1F8010A0u;
  constexpr std::uint32_t kDma2Control = 0x1F8010A8u;
  constexpr std::uint32_t kDma2LinkedListToGpu = 0x01000401u;
  core.mem_w32(kGp1, 0x00000000u);
  core.mem_w32(kGp0, 0xE3000000u);
  core.mem_w32(kGp0, 0xE4000000u | 1023u | (511u << 10));
  core.mem_w32(kGp0, 0xE5000000u);
  psx::gpu::sealRecord(core);
  core.mem_w32(kDma2Address, head);
  core.mem_w32(kDma2Control, kDma2LinkedListToGpu);
  return psx::gpu::sealRecord(core);
}

inline std::vector<psx::present::DrawPrimitive>
primitives(const psx::present::FrameRecord &record) {
  std::vector<psx::present::DrawPrimitive> found;
  for (const auto &entry : record.entries()) {
    if (const auto *primitive = std::get_if<psx::present::DrawPrimitive>(&entry)) {
      found.push_back(*primitive);
    }
  }
  return found;
}

// Every field a render owns, against the packet the guest's draw wrote.
inline psx::present::DrawPrimitive renderedFields(psx::present::DrawPrimitive primitive) {
  primitive.key = {};
  primitive.sourceAddress = {};
  return primitive;
}

inline bool sameFrame(const std::vector<psx::present::DrawPrimitive> &a,
                      const std::vector<psx::present::DrawPrimitive> &b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](const auto &x, const auto &y) {
           return renderedFields(x) == renderedFields(y);
         });
}

} // namespace spyro::test
