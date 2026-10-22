#include "spyro2/render/conversation_box.h"

#include "core.h"
#include "core/session.h"
#include "emission_scope.h"
#include "frame_state.h"
#include "gp0_command.h"
#include "gp0_primitive_decode.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "render/field/guest_packet.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro2/render/render_facts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

namespace spyro2 {
namespace {

// The return addresses of the name tab's three FUN_8004D2D8 calls.
constexpr std::array<std::uint32_t, 3> kTabRectReturns = {0x80013D48u, 0x80013D5Cu, 0x80013D70u};
// The draw mode FUN_8004D104(0x18) at 80013D28 sets for the tab: its GP0(E1) attribute.
constexpr std::uint16_t kTabDrawMode = 0x418;

struct BoxHeader {
  std::int32_t left = 0;
};

struct TabRect {
  std::int32_t left = 0;
  std::array<std::uint32_t, spyro::guest_packet::kF4Bytes / 4u - 1u> words{};
};
static_assert(std::is_trivially_copyable_v<TabRect>);

bool redrawing(Core &core) {
  return spyro::session(core).fieldRedraw.redrawing;
}

// How far the in-between draws from the later save: whole columns of the slide still to come.
int shiftOf(std::int32_t before, std::int32_t after, float t) {
  return static_cast<int>(std::lround(static_cast<float>(before - after) * (1.0f - t)));
}

BoxHeader boxHeaderOf(std::span<const std::byte> state) {
  if (state.size() < sizeof(BoxHeader) || (state.size() - sizeof(BoxHeader)) % 4u != 0) {
    lucent::error("conversation-box", "a saved box of {} bytes is not one", state.size());
    std::abort();
  }
  BoxHeader header;
  std::memcpy(&header, state.data(), sizeof(header));
  return header;
}

[[noreturn]] void notPackets(std::size_t offset, std::uint32_t word) {
  lucent::error("conversation-box",
                "the box's byte {}, 0x{:08X}, does not start a draw packet",
                offset,
                word);
  std::abort();
}

void shiftColumns(psx::present::DrawPrimitive &primitive, int shift) {
  for (psx::present::RecordVertex &vertex : primitive.vertices) {
    vertex.x += shift;
  }
}

void drawBox(Core *core) {
  if (redrawing(*core)) {
    return;
  }
  const auto left = static_cast<std::int32_t>(core->r[4]);
  const std::uint32_t first = core->mem_r32(kRenderGlobals.primitiveCursor);
  if (!psx::cpu::callOriginalOrPropagate(*core, ConversationBox::kEntry)) {
    return;
  }
  const std::uint32_t bytes = core->mem_r32(kRenderGlobals.primitiveCursor) - first;
  const std::optional<GuestAddressRange> ram = core->mappedMainRamRange(first, bytes);
  if (!ram || bytes % 4u != 0) {
    lucent::error("conversation-box", "the box wrote {} bytes from 0x{:08X}", bytes, first);
    std::abort();
  }
  const BoxHeader header{.left = left};
  const std::array<std::span<const std::byte>, 2> parts{
      std::as_bytes(std::span<const BoxHeader, 1>(&header, 1)),
      std::as_bytes(std::span<const std::uint8_t>(core->ram + ram->begin, bytes))};
  core->frameStates.save(core->emission.current(), parts);
}

void drawRect(Core *core) {
  const std::uint32_t site = core->r[31];
  if (redrawing(*core) || std::ranges::find(kTabRectReturns, site) == kTabRectReturns.end()) {
    (void)psx::cpu::callOriginalOrPropagate(*core, NameTabRect::kEntry);
    return;
  }
  const auto left = static_cast<std::int32_t>(core->r[4]);
  const std::uint32_t packet = core->mem_r32(kRenderGlobals.primitiveCursor);
  const psx::present::EmissionScope::Guard scope(core->emission, NameTabRect::kEntry, site, 0);
  if (!psx::cpu::callOriginalOrPropagate(*core, NameTabRect::kEntry)) {
    return;
  }
  TabRect state{.left = left};
  for (std::uint32_t word = 0; word < state.words.size(); ++word) {
    state.words[word] = core->mem_r32(packet + 4u * (word + 1u));
  }
  core->frameStates.save(core->emission.current(), state);
}

} // namespace

psx::present::StateProducer::Composition
ConversationBox::composition(const psx::present::StateInterval &states) const {
  return boxHeaderOf(states.from).left == boxHeaderOf(states.to).left ? Composition::Recorded
                                                                      : Composition::Render;
}

void ConversationBox::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  const int shift = shiftOf(boxHeaderOf(states.from).left, boxHeaderOf(states.to).left, states.t);
  const std::span<const std::byte> bytes = states.to.subspan(sizeof(BoxHeader));
  std::array<std::uint32_t, 16> packet{};
  psx::present::DrawModeTracker drawMode;
  for (std::size_t offset = 0; offset < bytes.size();) {
    std::uint32_t tag = 0;
    std::memcpy(&tag, bytes.data() + offset, sizeof(tag));
    const std::size_t words = tag >> 24;
    if (words > packet.size() || offset + 4u * (words + 1u) > bytes.size()) {
      notPackets(offset, tag);
    }
    std::memcpy(packet.data(), bytes.data() + offset + 4u, 4u * words);
    const std::span<const std::uint32_t> body(packet.data(), words);
    const std::size_t at = offset;
    offset += 4u * (words + 1u);
    if (words == 0 || drawMode.take(body)) {
      continue;
    }
    std::optional<psx::present::DrawPrimitive> primitive = psx::gpu::decodePacketPrimitive(body);
    if (!primitive) {
      notPackets(at, body[0]);
    }
    drawMode.apply(*primitive);
    shiftColumns(*primitive, shift);
    sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
  }
}

psx::present::StateProducer::Composition
NameTabRect::composition(const psx::present::StateInterval &states) const {
  return psx::present::stateAs<TabRect>(states.from).left ==
                 psx::present::stateAs<TabRect>(states.to).left
             ? Composition::Recorded
             : Composition::Render;
}

void NameTabRect::render(const psx::present::StateInterval &states,
                         psx::present::PrimitiveSink &sink) const {
  const TabRect before = psx::present::stateAs<TabRect>(states.from);
  const TabRect after = psx::present::stateAs<TabRect>(states.to);
  std::optional<psx::present::DrawPrimitive> primitive =
      psx::gpu::decodePacketPrimitive(after.words);
  if (!primitive) {
    notPackets(0, after.words[0]);
  }
  psx::gpu::applyTexPageAttribute(primitive->state, kTabDrawMode);
  shiftColumns(*primitive, shiftOf(before.left, after.left, states.t));
  sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
}

void installConversationBox(Core &core) {
  spyro::installProducerOverride(core,
                                 ConversationBox::kEntry,
                                 "spyro2-conversation-box",
                                 drawBox,
                                 std::make_unique<ConversationBox>(),
                                 psx::present::Arg::None);
  spyro::session(core).stateProducers.install(NameTabRect::kEntry, std::make_unique<NameTabRect>());
  psx::cpu::installNativeOverride(core, NameTabRect::kEntry, "spyro2-name-tab-rect", drawRect);
}

} // namespace spyro2
