// sprite_queue_producer.h — RasterizeSpritePrimQueue 0x80022A2C as a state producer: the
// override draws every queued record under its own emission scope and saves what the record was
// drawn from; the render draws one record again over host memory at t between two saves.
#pragma once

#include "render/field/guest_working_memory.h"
#include "render/terrain/terrain_rebuild.h"
#include "state_producer.h"

#include <array>
#include <cstdint>
#include <span>
#include <type_traits>

class Core;

namespace spyro1::actors {

// func_8001E6B8 (the title screen's demo and card draw) calls the queue from 0x8001E914 and, while
// g_TitlescreenState.m_State is TSS_Active, stores the overlay pair over the table's bin 0x20.
inline constexpr std::uint32_t kTitleDrawQueueReturn = 0x8001E91Cu;
inline constexpr std::uint32_t kTitleOverlayBin = 0x100u >> 3;

// One saved record: the camera and light it was drawn under, its bytes and HUD shift, and the slot
// its overlay chain is drawn from. The guest words its draw read follow it.
struct QueuedRecordState {
  spyro::guest_terrain::CameraState camera;
  std::array<std::uint32_t, 3> light{};
  std::uint32_t record = 0;
  std::int32_t hudShift = 0;
  psx::present::OtSlot overlaySlot;
  std::array<std::uint8_t, 0x58> bytes{};
};
static_assert(std::is_trivially_copyable_v<QueuedRecordState>);

class SpriteQueueProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x80022A2Cu;

  explicit SpriteQueueProducer(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
};

// The override and its render.
void registerSpriteQueueOverrides(Core &core);

} // namespace spyro1::actors
