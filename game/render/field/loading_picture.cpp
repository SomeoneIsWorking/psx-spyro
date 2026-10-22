#include "render/field/loading_picture.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "gp0_command.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "ordering_table.h"
#include "packet_chains.h"
#include "render/terrain/guest_depth_bins.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>

namespace spyro::loading_picture {
namespace {

// A page is a DR_MODE (tag and two words), then a SPRT (tag, colour, corner, uv, size).
constexpr std::uint32_t kDrawModeBytes = 0xC;
constexpr std::uint32_t kPageBytes = kDrawModeBytes + 0x14;
constexpr std::uint32_t kColourOffset = 4;

struct Header {
  std::uint32_t first = 0; // the first packet; the draw's packets follow the header
  std::uint8_t brightness = 0;
  std::array<std::uint8_t, 3> pad{};
};

constexpr std::uint32_t colourOf(std::uint32_t first, std::uint32_t page) {
  return first + page * kPageBytes + kDrawModeBytes + kColourOffset;
}

[[noreturn]] void notThePicture(std::uint32_t first, std::uint32_t bytes) {
  lucent::error("loadingpicture", "the {} bytes at 0x{:08X} are not its pages", bytes, first);
  std::abort();
}

Header headerOf(std::span<const std::byte> state, std::uint32_t pages) {
  if (state.size() < sizeof(Header) + pages * kPageBytes) {
    lucent::error("loadingpicture", "a saved picture of {} bytes is not one", state.size());
    std::abort();
  }
  Header header;
  std::memcpy(&header, state.data(), sizeof(header));
  return header;
}

} // namespace

void draw(Core &core, const Facts &facts, const guest_render_globals::Globals &globals) {
  if (core.mem_r32(facts.gate) != 0 || core.mem_r32(facts.counter) == 0) {
    (void)psx::cpu::callOriginalOrPropagate(core, facts.entry);
    return;
  }
  const std::uint32_t first = core.mem_r32(globals.primitiveCursor);
  const auto scope = core.emission.instance(facts.counter);
  (void)psx::cpu::callOriginalOrPropagate(core, facts.entry);
  const std::uint32_t bytes = core.mem_r32(globals.primitiveCursor) - first;
  const std::optional<GuestAddressRange> ram = core.mappedMainRamRange(first, bytes);
  if (bytes < facts.pages * kPageBytes || !ram) {
    notThePicture(first, bytes);
  }
  for (std::uint32_t page = 0; page < facts.pages; ++page) {
    if (!psx::gpu::Gp0Command(core.mem_r32(colourOf(first, page))).isRectangleOrSprite()) {
      notThePicture(first, bytes);
    }
  }
  const Header header{.first = first, .brightness = core.mem_r8(colourOf(first, 0))};
  const std::array<std::span<const std::byte>, 2> parts{
      std::as_bytes(std::span<const Header, 1>(&header, 1)),
      std::as_bytes(std::span<const std::uint8_t>(core.ram + ram->begin, bytes))};
  core.frameStates.save(core.emission.current(), parts);
}

void LoadingPicture::render(const psx::present::StateInterval &states,
                            psx::present::PrimitiveSink &sink) const {
  const Header before = headerOf(states.from, facts_.pages);
  const Header after = headerOf(states.to, facts_.pages);
  const std::span<const std::byte> packets = states.to.subspan(sizeof(Header));
  const auto bytes = static_cast<std::uint32_t>(packets.size());
  region_.clear();
  region_.provide(after.first, packets);
  const psx::present::EmitMemory memory(core_, region_);
  const auto brightness = static_cast<std::uint8_t>(
      psx::present::lerpInt(before.brightness, after.brightness, states.t));
  for (std::uint32_t page = 0; page < facts_.pages; ++page) {
    for (std::uint32_t channel = 0; channel < 3; ++channel) {
      memory.mem_w8(colourOf(after.first, page) + channel, brightness);
    }
  }
  psx::present::PacketChainWalker walker(memory,
                                         psx::present::PacketBound::within({after.first, bytes}));
  psx::present::DrawModeTracker drawMode;
  // Each list's last packet links on to whatever the frame appended to it next.
  const std::uint32_t text = after.first + facts_.pages * kPageBytes;
  const std::uint32_t lastPage = text - (kPageBytes - kDrawModeBytes);
  memory.mem_w32(lastPage, memory.mem_r32(lastPage) | psx::gpu::kOtChainEnd);
  walker.emitChain(after.first, facts_.pageSlot, drawMode, sink);
  if (bytes == text - after.first) {
    return;
  }
  const std::uint32_t end = psx::gpu::mainRamOffsetOf(after.first) + bytes;
  std::uint32_t last = text;
  for (;;) {
    const psx::gpu::OtNode node(psx::gpu::mainRamOffsetOf(last),
                                psx::gpu::OtHeaderWord{memory.mem_r32(last)});
    // The draw appends in address order, so a link back or out of its packets is not its own.
    if (node.endsChain() || node.nextRamOffset() <= node.ramOffset() ||
        node.nextRamOffset() >= end) {
      break;
    }
    last = psx::gpu::guestAddressOf(node.nextRamOffset());
  }
  memory.mem_w32(last, memory.mem_r32(last) | psx::gpu::kOtChainEnd);
  walker.emitChain(text, depth_bins::kOverlaySlot, drawMode, sink);
}

} // namespace spyro::loading_picture
