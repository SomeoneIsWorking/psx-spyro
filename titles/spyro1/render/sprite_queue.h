// sprite_queue.h — SCUS_942.28's RasterizeSpritePrimQueue 0x80022A2C: the lit and HUD mobys
// queued at kSpriteQueue. Each record is culled, projected, lit and drawn into the local bins,
// whose chain is then linked into one slot: its depth bin, or the overlay pair for a HUD record at
// depth 0. Runs over guest memory for the real field and over host memory for an in-between.
#pragma once

#include "projected_vertex.h"
#include "render/field/guest_moby_frustum.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_screen.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/local_bins.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro1::actors {

// What the presentation asks of the queue: the sphere cull's slope and the outcode window.
struct QueueView {
  spyro::guest_moby_frustum::HorizontalSlope slope;
  spyro::guest_terrain::ScreenBounds bounds{512, 0, {}};
};

// The view the presented plan asks for now.
[[nodiscard]] QueueView presentedQueueView(const Core &core);

// One record's chain, from its deepest local bin to its nearest, and where it is linked.
struct QueuedChain {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
  std::uint32_t depthBin = 0;
  bool overlay = false;       // a HUD record at depth 0 goes to the overlay pair
  std::uint32_t drawMode = 0; // non-zero: a DR_MODE with blend drawMode - 1 leads the chain

  // 80023A08: that DR_MODE's command word, dithered.
  [[nodiscard]] std::uint32_t modeCommand() const {
    return 0xE1000000u | 0x600u | ((drawMode - 1) << 5);
  }
};

class SpriteQueue {
public:
  SpriteQueue(Core &core, const psx::present::EmitMemory &memory, const QueueView &view);

  // 80022A64..80022B04: the camera into the scratchpad, the light into the colour matrix.
  void begin();
  // 80022B20..800239B4: one record drawn into the local bins and chained, a HUD record `hudShift`
  // columns off its authored x, its angles refined by an in-between's `angleFractions`
  // (spyro::moby_record::Between). Nullopt when it was culled or linked nothing.
  std::optional<QueuedChain>
  draw(std::uint32_t record, std::int32_t hudShift, std::uint32_t angleFractions);
  // 800239B8..80023A74: the chain into the guest's ordering table, behind its draw mode.
  void link(const QueuedChain &chain);
  // 80023A78: the shadow list's end and the registers retail leaves.
  void end();

private:
  // A record's view-space centre, after the culls.
  struct Centre {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    std::uint32_t screenSpace = 0;
    bool seenAtRetail = true; // retail's slope keeps it too, so it may set +0x51
  };

  // A record as draw() read it, before it clears the drawn byte inside the cull halfword.
  struct Placement {
    std::uint32_t record = 0;
    std::uint32_t cull = 0;
    std::uint32_t x = 0;
  };
  // A polygon's corner word and colour word.
  struct PolygonWords {
    std::uint32_t corners = 0;
    std::uint32_t colours = 0;
  };
  // A polygon's colour word and the screen slots of its third and fourth corners (a0, a1).
  struct PolygonTail {
    std::uint32_t colours = 0;
    std::uint32_t third = 0;
    std::uint32_t fourth = 0;
  };

  [[nodiscard]] std::optional<Centre> placeInWorld(const Placement &placement);
  [[nodiscard]] Centre placeOnScreen(const Placement &placement, std::int32_t shift);
  void orient(const Centre &centre, std::uint32_t record);
  [[nodiscard]] bool projectVertices();
  void lightVertices(std::uint32_t record);
  void drawPolygon(const PolygonWords &polygon);
  void drawGouraud(const PolygonTail &tail);
  void drawLit(const PolygonTail &tail);
  void drawFlat(const PolygonTail &tail);
  [[nodiscard]] std::optional<QueuedChain> chainLocalBins();

  Core &core_;
  const psx::present::EmitMemory &memory_;
  QueueView view_;
  // Retail's registers that outlive one step: v0, v1, the shadow cursor (sp), the edge class and
  // draw mode (s7), the mesh (s6), the angles (s4), the depth bin (t9) and bias (t8), the packet
  // cursor, the vertex colours (s4 again), the palette word (LO), the four packed screen words,
  // and the bins the record's packets reached (s2 lowest, s3 highest).
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
  std::uint32_t shadows_ = 0;
  std::uint32_t edge_ = 0;
  std::uint32_t mesh_ = 0;
  std::uint32_t angles_ = 0;
  std::uint32_t angleFractions_ = 0;
  psx::gte::RotationWords rotation_{};
  std::uint32_t depthBin_ = 0;
  std::uint32_t depthBias_ = 0;
  std::uint32_t vertexColours_ = 0;
  std::uint32_t paletteWord_ = 0;
  std::array<psx::gte::ProjectedVertex, 4> screen_{};
  LocalBins bins_;
};

} // namespace spyro1::actors
