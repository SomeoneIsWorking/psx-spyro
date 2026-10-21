// portals.h — SCUS_942.28's portal drawer 0x80050BD0: each level portal's outline projected, and
// from afar its opening filled with flat triangles clipped to that outline (0x8004FEA0); near one
// the portal's own sky is drawn through it (0x8004F4BC, 0x80050240), each sky polygon cut to the
// outline (0x8004F7E8). Runs over guest memory for the real field and over host memory for an
// in-between.
#pragma once

#include "gte_registers.h"
#include "gte_state.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_camera_builder.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/sky.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

class Core;

namespace psx::present {
class PrimitiveSink;
}

namespace spyro1::portals {

// Portal (cyclorama.h): its sky, point count, plane normal, world sector, then its points.
namespace portal {
inline constexpr std::uint32_t kSky = 0x00;
inline constexpr std::uint32_t kPointCount = 0x04;
inline constexpr std::uint32_t kNormal = 0x08;
inline constexpr std::uint32_t kSector = 0x14;
inline constexpr std::uint32_t kPoints = 0x20;
inline constexpr std::uint32_t kPointBytes = 0x0C;
} // namespace portal
// The portal sky's flat colour (PortalDataSky::copyBgColor).
inline constexpr std::uint32_t kSkyColour = 0x10;

// D_80078DD8: each point's screen x, y and depth, a word each, 16 points at most.
inline constexpr std::uint32_t kProjected = 0x80078DD8u;
inline constexpr std::uint32_t kProjectedBytes = 0xC0u;
// D_80077EA0: the outline's edges, 0x18 bytes each, ended by a zero flag.
inline constexpr std::uint32_t kEdges = 0x80077EA0u;
inline constexpr std::uint32_t kEdgesBytes = 0x108u;
inline constexpr std::uint32_t kEdgeBytes = 0x18u;
// D_80075934: the depth bin the fill is linked into.
inline constexpr std::uint32_t kBin = 0x80075934u;
// The fill queues the pieces a clip splits off here, four words each (8004FF00).
inline constexpr std::uint32_t kPieces = actors::kWorkBlock + 0x800u;
inline constexpr std::uint32_t kPieceBytes = 0x10u;
// D_80075798: where the clipped pieces the near sky queues start.
inline constexpr std::uint32_t kPieceQueue = 0x80075798u;
// D_8007AA00: the outline's bounding window, top, bottom, right and left, each << 16.
inline constexpr std::uint32_t kWindow = 0x8007AA00u;
inline constexpr std::uint32_t kWindowBytes = 0x10u;
// D_8007575C and D_800757D4: how far into the fade band the portal is, and the colour it fades to.
inline constexpr std::uint32_t kFadeStep = 0x8007575Cu;
inline constexpr std::uint32_t kFadeColour = 0x800757D4u;
// D_80075858 and D_800758FC: the near sky's yaw and pitch spin.
inline constexpr std::uint32_t kSkyYaw = 0x80075858u;
inline constexpr std::uint32_t kSkyPitch = 0x800758FCu;

// Past this a portal is only filled; below it its sky is drawn too, faded in over the last 0x1000.
inline constexpr std::int32_t kNearDistance = 0x4000;
inline constexpr std::int32_t kFadeDistance = 0x3000;

// What the near sky reuses from the far pass.
struct Approach {
  std::uint32_t portal = 0;
  std::uint32_t distance = 0;
  std::uint32_t colour = 0;
  std::uint32_t winding = 0;
  std::array<std::uint32_t, 3> centre{};
  std::array<std::uint32_t, 2> average{};
};

// The near sky's spin, a 12-bit yaw and pitch.
struct SkySpin {
  std::uint32_t yaw = 0;
  std::uint32_t pitch = 0;
};

class Portals final : public sky::TriangleCut {
public:
  // `places`, when given, is told each emitted vertex's unrounded place.
  Portals(Core &core,
          const psx::present::EmitMemory &memory,
          psx::present::PrimitiveSink *places = nullptr);

  // 80050E90..800518CC for portal `index`: the outline and, when it shows, the fill. Nullopt when
  // the portal is skipped whole.
  std::optional<Approach> drawFar(std::uint32_t index);
  // 800518CC..80051F5C: within kNearDistance, the outline pushed out and the portal's sky drawn
  // through it with `rotations`.
  void drawNear(const Approach &approach, const sky::Rotations &rotations);

  void cut(const sky::SkyTriangle &triangle, sky::Cyclorama &sky) override;
  void finishSector(sky::Cyclorama &sky) override;

private:
  struct Box {
    std::int32_t minX = 0;
    std::int32_t maxX = 0;
    std::int32_t minY = 0;
    std::int32_t maxY = 0;
  };
  struct Point {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
  };

  // A screen point, the colour word carried with it and its unrounded place.
  struct Corner {
    std::uint32_t point = 0;
    std::uint32_t colour = 0;
    GtePlace place{};
  };
  // The outline's points, edges and queued pieces as kProjected, kEdges and kPieces lay them out.
  static constexpr std::size_t kOutlinePoints = kProjectedBytes / portal::kPointBytes;
  static constexpr std::size_t kPieceSlots =
      (actors::kWorkBlock + actors::kWorkBlockBytes - kPieces) / kPieceBytes;

  [[nodiscard]] Point projected(std::uint32_t i) const;
  [[nodiscard]] std::uint32_t
  neighbour(std::uint32_t i, std::uint32_t count, std::uint32_t winding) const;
  // The edge from point i to j appended at `edges` unless both lie past one screen side; their
  // spans in x and y when it was.
  std::optional<std::array<std::uint32_t, 2>>
  appendEdge(std::uint32_t i, std::uint32_t j, std::uint32_t &edges);
  [[nodiscard]] std::uint32_t
  coverage(const Box &box, std::uint32_t edges, std::uint32_t onScreen) const;
  [[nodiscard]] bool facesCamera(const std::array<std::uint32_t, 3> &centre) const;
  void publishWindow(Box box, std::uint32_t edges);
  // The outline pushed out; its sky and whether it fades, when it shows.
  std::optional<std::pair<std::uint32_t, bool>> outlineNear(const Approach &approach);
  // 0x8004F4BC, or 0x80050240 when `fading`: sky `sky` through the outline, linked into the
  // portal's bin.
  void drawSky(std::uint32_t sky, bool fading, const sky::Rotations &rotations);
  // 0x8004FEA0: the two screen triangles clipped to the outline, flat in `colour`.
  void fill(std::uint32_t colour);
  void clip(std::array<Corner, 3> corners,
            std::uint32_t edge,
            std::uint32_t &queued,
            std::uint32_t &cursor,
            std::uint32_t command,
            std::uint32_t slot);
  // 0x8004F7E8: a sky triangle cut by each edge from `edge` on, then drawn.
  void cutShaded(std::array<Corner, 3> corners, std::uint32_t edge, sky::Cyclorama &sky);
  // NCLIP's verdict against `edge`: bit 3 + n set for each corner n outside it.
  [[nodiscard]] std::uint32_t outsideCorners(const std::array<std::uint32_t, 3> &points,
                                             std::uint32_t edge);
  // 0x8004FDAC: where the segment from `inside` to `outside` crosses `edge`, and the colour there.
  [[nodiscard]] Corner crossing(Corner inside, Corner outside, std::uint32_t edge);
  // The same crossing on the edge's unrounded line, between the two places.
  [[nodiscard]] GtePlace crossingPlace(GtePlace inside, GtePlace outside, std::uint32_t edge) const;
  // The places kept for the piece queued at `piece`, `bytes` a piece.
  std::array<GtePlace, 3> &piecePlaces(std::uint32_t piece, std::uint32_t bytes);
  // ColorLerp 0x80017E54.
  [[nodiscard]] std::uint32_t
  colourToward(std::uint32_t colour, std::uint32_t target, std::uint32_t amount);
  // func_80016D08: the highest set bit, 0 for 0.
  [[nodiscard]] std::uint32_t topBit(std::uint32_t value);

  Core &core_;
  const psx::present::EmitMemory &memory_;
  psx::present::PrimitiveSink *places_;
  // Kept while `places_` takes them.
  std::array<GtePlace, kOutlinePoints> outline_{};
  std::array<std::array<GtePlace, 2>, kOutlinePoints> edgeEnds_{};
  std::array<std::array<GtePlace, 3>, kPieceSlots> pieces_{};
};

// 80050BD0..80050C58: the near sky's spin advanced, as the drawer does once a call.
SkySpin advanceSkySpin(Core &core);
// 80050C5C..80050E8C: the near sky's rotations, the camera's own turned by the spin.
sky::Rotations
nearSkyRotations(Core &core, const spyro::guest_camera::Angles &camera, SkySpin spin);

} // namespace spyro1::portals
