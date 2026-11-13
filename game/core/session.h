#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/archive_transfer.h"
#include "core/debug/warp.h"
#include "core/field_owner.h"
#include "emit_memory.h"
#include "game_runtime.h"
#include "image_identity.h"
#include "render/packet_layers.h"

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
  // The title's `warp` request, applied by its frame driver.
  std::unique_ptr<Warp> warp;
  std::unique_ptr<TitleState> titleState;
  // The frame's crop widescreen UI and 2D font glyphs, named by the title's linkers.
  PacketLayers layers;
};

// The session of the Game `core` belongs to; a native override reaches it from its Core alone.
Session &session(Core &core);
const Session &session(const Core &core);

} // namespace spyro
