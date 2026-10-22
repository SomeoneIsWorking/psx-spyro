// paired_actor.h — SCUS_942.28's RasterizePairedActor 0x80023AC4, which draws Spyro: three
// parts, each a frame blended toward a partner frame and turned by its own angles, decoded from
// delta streams, projected into the scratchpad, drawn into the local bins (cut at the water surface
// while he is in water) and merged into the ordering table. Runs over guest memory for the real
// field and over host memory for an in-between.
#pragma once

#include "projected_vertex.h"
#include "render/field/guest_working_memory.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/local_bins.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

class Core;

namespace spyro1::actors {

class PairedActor {
public:
  PairedActor(Core &core, const psx::present::EmitMemory &memory);

  // 80023AFC..800258AC. False when the centre was culled or no polygon reached a bin.
  bool draw();
  // 800258B0: the registers retail leaves.
  void end();

private:
  // Two packed offsets per part pair: x from bit 21, y from bit 10, z the low ten bits doubled.
  struct Offsets {
    std::array<std::uint32_t, 3> first{};
    std::array<std::uint32_t, 3> second{};
  };

  void stageParts();
  [[nodiscard]] Offsets blendOffsets();
  [[nodiscard]] bool placeCentre();
  void orientParts(const Offsets &offsets);
  void rotate(std::uint32_t angles);
  void writeRotation(std::uint32_t at) const;
  void loadRotation();

  void projectParts();
  void projectPlain(std::uint32_t &words);
  void projectBlended(std::uint32_t &words);
  [[nodiscard]] bool stepOn();
  std::uint32_t keepInput();
  void loadVertex();
  void keepProjection();
  void measureHeights();
  void tintColours();

  void beginPolygons();
  void drawWhole();
  // A whole-pass polygon: its colour word (s0), third and fourth corners (a0, a1), the colour
  // addresses of its first, second and fourth corners (t2, t3, t5) and its entry's end (ra).
  struct WholePolygon {
    std::uint32_t colourWord = 0;
    std::uint32_t third = 0;
    std::uint32_t fourth = 0;
    std::array<std::uint32_t, 3> colours{};
    std::uint32_t end = 0;
  };
  // A cut-pass triangle: the polygon's words (s1, s2), and per corner its vertex entry offset,
  // colour offset and uv word.
  struct CutTriangle {
    std::uint32_t word = 0;
    std::uint32_t colourWord = 0;
    std::array<std::uint32_t, 3> corners{};
    std::array<std::uint32_t, 3> colourOffsets{};
    std::array<std::uint32_t, 3> uvs{};
  };
  // An edge crossing the surface: each end's height, and its colour, screen word and uv word.
  struct CutEdge {
    std::array<std::uint32_t, 2> heights{};
    std::array<std::uint32_t, 3> from{};
    std::array<std::uint32_t, 3> to{};
    std::array<GtePlace, 2> places{}; // the ends' unrounded places
  };

  void emitWholeQuad(const WholePolygon &polygon);
  // `backHalf`: a quad's back half, whose first uv halfword is its fourth's.
  void emitWholeTriangle(const WholePolygon &polygon, bool backHalf);
  void drawCut();
  void cutTriangle(const CutTriangle &triangle);
  // The crossing written at written_, its place blended as its screen word is.
  void cutEdge(const CutEdge &edge);
  // The local bin a polygon's depth sum falls in; `shifted` takes the shifted sum either way.
  [[nodiscard]] std::optional<std::uint32_t> binOf(std::uint32_t sum, std::uint32_t &shifted) const;
  [[nodiscard]] bool mergeBins();

  Core &core_;
  const psx::present::EmitMemory &memory_;

  // Retail's registers that outlive one step.
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
  std::uint32_t model_ = 0;                           // fp
  std::uint32_t header_ = 0;                          // part 0's animation header, parked in HI
  std::uint32_t blend_ = 0;                           // t6: part 0's weight << 8
  std::uint32_t depthShift_ = 0;                      // s6, then s7 once the partner's is known
  std::uint32_t partnerDepthShift_ = 0;               // s7
  std::array<std::uint32_t, 2> offsetWords_{};        // t2, t3
  std::array<std::uint32_t, 2> partnerOffsetWords_{}; // t4, t5
  std::array<std::uint32_t, 3> centre_{};             // view space
  std::array<std::uint32_t, 5> rotation_{};           // a3, t0..t3
  // The vertex decode's streams and cursors.
  std::uint32_t screen_ = 0;                      // fp
  std::uint32_t vertex_ = 0;                      // sp
  std::uint32_t end_ = 0;                         // ra
  std::uint32_t last_ = 0;                        // LO
  std::uint32_t format_ = 0;                      // t0
  std::uint32_t partnerFormat_ = 0;               // s0
  std::array<std::uint32_t, 3> position_{};       // a1..a3
  std::array<std::uint32_t, 3> partner_{};        // t5..t7
  std::array<std::uint32_t, 3> partnerStreams_{}; // t1..t3
  std::uint32_t weight_ = 0;                      // s7 in the decode
  std::uint32_t waterLevel_ = 0;                  // t9
  // The polygon passes.
  std::uint32_t colours_ = 0;    // t9 after the decode
  LocalBins bins_;               // t8, s4, s5
  std::uint32_t depthBase_ = 0;  // s6
  std::uint32_t binShift_ = 0;   // s7
  std::uint32_t polygon_ = 0;    // ra
  std::uint32_t polygonEnd_ = 0; // fp
  std::uint32_t written_ = 0;    // s0 in a cut polygon
};

} // namespace spyro1::actors
