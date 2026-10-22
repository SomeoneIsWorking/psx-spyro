#include "spyro2/render/conversation.h"

#include "core.h"
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
#include <type_traits>

namespace spyro2 {
namespace {

// The return addresses of the name tab's three FUN_8004D2D8 calls.
constexpr std::array<std::uint32_t, 3> kTabRectReturns = {0x80013D48u, 0x80013D5Cu, 0x80013D70u};
// The draw mode FUN_8004D104(0x18) at 80013D28 sets for the tab: its GP0(E1) attribute.
constexpr std::uint16_t kTabDrawMode = 0x418;
// The underline's FUN_8004D1BC call.
constexpr std::uint32_t kUnderlineReturn = 0x80013DB8u;
// FUN_8004A374's calls for the name, the lines, the choices, the prompts and the orb message; not
// 80014350, which draws one string five times over.
constexpr std::array<std::uint32_t, 7> kTextReturns = {0x80013DD0u,
                                                       0x80013E84u,
                                                       0x80013F84u,
                                                       0x80013FE4u,
                                                       0x80014228u,
                                                       0x8001429Cu,
                                                       0x800144D4u};

struct BoxHeader {
  std::int32_t left = 0;
};

struct TabRect {
  std::int32_t left = 0;
  std::array<std::uint32_t, spyro::guest_packet::kF4Bytes / 4u - 1u> words{};
};
static_assert(std::is_trivially_copyable_v<TabRect>);

struct TextHeader {
  std::uint32_t drawn = 1;
};

template <class Header> Header headerOf(std::span<const std::byte> state) {
  if (state.size() < sizeof(Header) || (state.size() - sizeof(Header)) % 4u != 0) {
    lucent::error("conversation", "a saved draw of {} bytes is not one", state.size());
    std::abort();
  }
  Header header;
  std::memcpy(&header, state.data(), sizeof(header));
  return header;
}

// How far the in-between draws from the later save: whole columns of the slide still to come.
int shiftOf(std::int32_t before, std::int32_t after, float t) {
  return static_cast<int>(std::lround(static_cast<float>(before - after) * (1.0f - t)));
}

[[noreturn]] void notPackets(std::size_t offset, std::uint32_t word) {
  lucent::error("conversation",
                "the saved byte {}, 0x{:08X}, does not start a draw packet",
                offset,
                word);
  std::abort();
}

void shiftColumns(psx::present::DrawPrimitive &primitive, int shift) {
  for (psx::present::RecordVertex &vertex : primitive.vertices) {
    vertex.x += shift;
  }
}

// Runs the draw at `entry` and saves `header` with the packets it wrote from the primitive cursor,
// under the open scope. Nothing is saved for a draw that wrote no packet.
template <class Header> void saveDraw(Core &core, std::uint32_t entry, const Header &header) {
  const std::uint32_t first = core.mem_r32(kRenderGlobals.primitiveCursor);
  if (!psx::cpu::callOriginalOrPropagate(core, entry)) {
    return;
  }
  const std::uint32_t bytes = core.mem_r32(kRenderGlobals.primitiveCursor) - first;
  if (bytes == 0) {
    return;
  }
  const std::optional<GuestAddressRange> ram = core.mappedMainRamRange(first, bytes);
  if (!ram || bytes % 4u != 0) {
    lucent::error("conversation", "0x{:08X} wrote {} bytes from 0x{:08X}", entry, bytes, first);
    std::abort();
  }
  const std::array<std::span<const std::byte>, 2> parts{
      std::as_bytes(std::span<const Header, 1>(&header, 1)),
      std::as_bytes(std::span<const std::uint8_t>(core.ram + ram->begin, bytes))};
  core.frameStates.save(core.emission.current(), parts);
}

// The packets of a saved draw into the overlay slot, moved `shift` columns.
void emitPackets(std::span<const std::byte> bytes, int shift, psx::present::PrimitiveSink &sink) {
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
    std::optional<psx::present::DrawPrimitive> primitive = psx::gpu::decodePacketPrimitive({.words = body, .places = {}});
    if (!primitive) {
      notPackets(at, body[0]);
    }
    drawMode.apply(*primitive);
    shiftColumns(*primitive, shift);
    sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
  }
}

void drawBox(Core *core) {
  saveDraw(*core,
           ConversationBox::kEntry,
           BoxHeader{.left = static_cast<std::int32_t>(core->r[4])});
}

void drawRect(Core *core) {
  const std::uint32_t site = core->r[31];
  if (std::ranges::find(kTabRectReturns, site) == kTabRectReturns.end()) {
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

void drawUnderline(Core *core) {
  if (core->r[31] != kUnderlineReturn) {
    (void)psx::cpu::callOriginalOrPropagate(*core, ConversationText::kUnderlineEntry);
    return;
  }
  const psx::present::EmissionScope::Guard scope(
      core->emission, ConversationText::kUnderlineEntry, kUnderlineReturn, 0);
  saveDraw(*core, ConversationText::kUnderlineEntry, TextHeader{});
}

void drawText(Core *core) {
  if (std::ranges::find(kTextReturns, core->r[31]) == kTextReturns.end()) {
    (void)psx::cpu::callOriginalOrPropagate(*core, ConversationText::kTextEntry);
    return;
  }
  const psx::present::EmissionScope::Guard scope(
      core->emission, ConversationText::kTextEntry, core->r[4], 0);
  saveDraw(*core, ConversationText::kTextEntry, TextHeader{});
}

} // namespace

psx::present::StateProducer::Composition
ConversationBox::composition(const psx::present::StateInterval &states) const {
  return headerOf<BoxHeader>(states.from).left == headerOf<BoxHeader>(states.to).left
             ? Composition::Recorded
             : Composition::Render;
}

void ConversationBox::render(const psx::present::StateInterval &states,
                             psx::present::PrimitiveSink &sink) const {
  emitPackets(
      states.to.subspan(sizeof(BoxHeader)),
      shiftOf(headerOf<BoxHeader>(states.from).left, headerOf<BoxHeader>(states.to).left, states.t),
      sink);
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
      psx::gpu::decodePacketPrimitive({.words = after.words, .places = {}});
  if (!primitive) {
    notPackets(0, after.words[0]);
  }
  psx::gpu::applyTexPageAttribute(primitive->state, kTabDrawMode);
  shiftColumns(*primitive, shiftOf(before.left, after.left, states.t));
  sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
}

psx::present::StateProducer::Composition
ConversationText::composition(const psx::present::StateInterval &states) const {
  return std::ranges::equal(states.from, states.to) ? Composition::Recorded : Composition::Render;
}

void ConversationText::render(const psx::present::StateInterval &states,
                              psx::present::PrimitiveSink &sink) const {
  if (headerOf<TextHeader>(states.from).drawn == 0 || headerOf<TextHeader>(states.to).drawn == 0) {
    return;
  }
  emitPackets(states.from.subspan(sizeof(TextHeader)), 0, sink);
}

bool ConversationText::rests(std::span<const std::byte> drawn, std::span<std::byte> into) const {
  std::ranges::copy(drawn, into.begin());
  const TextHeader rest{.drawn = 0};
  std::memcpy(into.data(), &rest, sizeof(rest));
  return true;
}

void installConversation(Core &core) {
  spyro::installProducerOverride(core,
                                 ConversationBox::kEntry,
                                 "spyro2-conversation-box",
                                 drawBox,
                                 std::make_unique<ConversationBox>(),
                                 psx::present::Arg::None);
  spyro::session(core).stateProducers.install(NameTabRect::kEntry, std::make_unique<NameTabRect>());
  psx::cpu::installNativeOverride(core, NameTabRect::kEntry, "spyro2-name-tab-rect", drawRect);
  for (const std::uint32_t entry :
       {ConversationText::kUnderlineEntry, ConversationText::kTextEntry}) {
    spyro::session(core).stateProducers.install(entry, std::make_unique<ConversationText>());
  }
  psx::cpu::installNativeOverride(
      core, ConversationText::kUnderlineEntry, "spyro2-conversation-underline", drawUnderline);
  psx::cpu::installNativeOverride(
      core, ConversationText::kTextEntry, "spyro2-conversation-text", drawText);
}

} // namespace spyro2
