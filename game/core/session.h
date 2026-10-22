#pragma once

#include <memory>
#include <vector>

#include "core/archive_transfer.h"
#include "core/debug/warp.h"
#include "core/field_owner.h"
#include "emit_memory.h"
#include "game_runtime.h"
#include "image_identity.h"
#include "render/field/field_redraw.h"
#include "state_producer.h"

class Core;

namespace spyro {

// Per-Core state that one title's natives own.
class TitleState {
public:
  virtual ~TitleState() = default;
};

// The per-Core state of every Spyro title, created with each Game by Runtime::createSession.
class Session final : public psx::game::TitleSessionState {
public:
  ArchiveTransfer archiveTransfer{};
  // The selected title's field owner, published by its frame driver. Non-owning: the driver
  // holds it and is installed before the first host turn can arrive.
  FieldOwner *fieldOwner = nullptr;
  // The sector-visibility bytes the guest's own visibility call left in the scratchpad. The
  // drawer's classification clears each byte as it consumes it, so an in-between has to be handed
  // them rather than find them.
  std::vector<std::uint8_t> terrainVisibility;
  // How far the real field's packets could grow before its far pass stopped: the pool's top minus
  // the cursor the drawer started at. An in-between draws within the same budget.
  std::int32_t terrainPacketBudget = 0;
  // The field dispatcher's last inputs, for the redraw of the next in-between.
  field_redraw::History fieldRedraw;
  // The level words a producer's draw reads, kept across saves.
  psx::present::Kept<psx::present::LevelReads> drawReads;
  // The host memory a producer's in-between runs over, kept across renders.
  psx::present::Kept<psx::present::HostMemory> renderMemory;
  // The bins a terrain in-between linked, kept across renders.
  psx::present::Kept<std::vector<std::uint8_t>> terrainBins;
  // The state renders this title's producers registered, keyed by producer entry.
  psx::present::StateProducerTable stateProducers;
  // The screen draws installed in overlays, keyed to the image each landed as.
  std::vector<psx::cpu::NativeKey> screenDrawOverrides;
  // The title's `warp` request, applied by its frame driver.
  std::unique_ptr<Warp> warp;
  std::unique_ptr<TitleState> titleState;
};

// The session of the Game `core` belongs to; a native override reaches it from its Core alone.
Session &session(Core &core);
const Session &session(const Core &core);

} // namespace spyro
