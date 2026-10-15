// spyro1_actor_emitter.h — SCUS_942.28's moby emitters: EmitActorDrawList 0x8001F798 draws the
// draw list the list builder wrote, and EmitSecondaryActorPrimitives 0x80020F34 the secondary list,
// lit per face. Per entry: the frame's near detail patched in, every vertex decoded (blended toward
// a partner frame) and projected into the scratchpad, the colours faded by distance, the polygons
// drawn into the local bins and the bins merged into the ordering table. Runs over guest memory for
// the real field and over host memory for an in-between.
#pragma once

#include "guest_terrain_screen.h"
#include "guest_working_memory.h"
#include "spyro1_actor_facts.h"
#include "spyro1_local_bins.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro1::actors {

class ActorEmitter {
public:
  ActorEmitter(Core &core,
               spyro::guest_memory::WorkingMemory &memory,
               const spyro::guest_terrain::ScreenBounds &bounds);
  virtual ~ActorEmitter() = default;
  ActorEmitter(const ActorEmitter &) = delete;
  ActorEmitter &operator=(const ActorEmitter &) = delete;
  ActorEmitter(ActorEmitter &&) = delete;
  ActorEmitter &operator=(ActorEmitter &&) = delete;

  // The packet cursor and the fog switch; the caller's $v1.
  void begin(std::uint32_t v1);
  // One draw-list entry. False when it was culled or linked nothing.
  bool draw(std::uint32_t entry);
  // The packet cursor back, and the registers retail leaves.
  void end();

protected:
  // A projected vertex as the vertex loop reads it before the next decode.
  struct Projected {
    std::uint32_t screen = 0; // SXY2
    std::uint32_t depth = 0;  // SZ3
    std::array<std::uint32_t, 3> view{};
  };

  [[nodiscard]] Projected readProjected();
  // Where the vertex loop keeps each vertex's depth, for `count` vertices.
  [[nodiscard]] virtual std::uint32_t depthTable(std::uint32_t count) const;
  // Steps the depth slot and keeps the vertex's depth.
  virtual void keepDepth(const Projected &vertex);
  // The polygon pass, from the colour fade's polygon list and colours.
  virtual void drawPolygons();
  // The bins, the depth base and shift, and the polygon list's end, with the depths at `depths`.
  void beginPolygons(std::uint32_t depths);

  // A triangle's colour word, corner slots, first corner's colour address, and whether it is a
  // quad's back half (its first uv halfword then the fourth corner's) or flagged for lighting.
  struct Triangle {
    std::uint32_t colourWord = 0;
    std::array<std::uint32_t, 3> slots{};
    std::uint32_t firstColour = 0;
    bool textured = false;
    bool backHalf = false;
    bool lit = false;
  };
  // A triangle's three corner colours, the first without its command byte.
  struct TriangleColours {
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    std::uint32_t third = 0;
  };
  // The halves of a quad that face the camera.
  enum class QuadFaces : std::uint8_t { None, Front, Back, Whole };
  // From the first NCLIP's result, with the fourth corner's screen word for the second.
  [[nodiscard]] virtual QuadFaces faceQuad(bool twoSided, std::uint32_t fourthScreen);
  // The depth kept for the vertex at a corner slot.
  [[nodiscard]] virtual std::uint32_t depthAt(std::uint32_t slot) const;
  // A facing triangle's colours, packet and bin.
  virtual void drawTriangleTail(const Triangle &triangle);
  [[nodiscard]] TriangleColours readColours(const Triangle &triangle) const;
  void linkTriangle(const Triangle &triangle, const TriangleColours &colours);

  Core &core_;
  spyro::guest_memory::WorkingMemory &memory_;
  spyro::guest_terrain::ScreenBounds bounds_;
  LocalBins bins_;

  // Retail's registers that outlive one step.
  std::uint32_t v1_ = 0;
  std::uint32_t entry_ = 0;     // gp
  std::uint32_t flags_ = 0;     // s3: edge class | depth shift | blend | z step | scale
  std::uint32_t frame_ = 0;     // LO
  std::uint32_t weight_ = 0;    // s4: the blend toward the partner frame
  std::uint32_t halfShift_ = 0; // s5: the halfword deltas' scale
  // The vertex loop.
  std::uint32_t words_ = 0;         // v0
  std::uint32_t halves_ = 0;        // at
  std::uint32_t partnerWords_ = 0;  // t0
  std::uint32_t partnerHalves_ = 0; // a3
  std::uint32_t depth_ = 0;         // sp
  std::uint32_t screen_ = 0;        // fp
  std::uint32_t screenEnd_ = 0;     // ra
  std::uint32_t outside_ = 0;       // s6: the outcodes every vertex shares
  // The polygon pass.
  std::uint32_t fogClut_ = 0;    // s2: added to a textured polygon's first uv word
  std::uint32_t polygon_ = 0;    // fp
  std::uint32_t polygonEnd_ = 0; // ra
  std::uint32_t word_ = 0;       // at: the polygon's first word
  std::uint32_t colours_ = 0;    // t9
  std::uint32_t depths_ = 0;     // gp
  std::uint32_t depthBase_ = 0;  // s6
  std::uint32_t binShift_ = 0;   // s7, the edge class in its sign
  std::uint32_t binsBase_ = 0;   // s3

private:
  [[nodiscard]] bool place();
  void patchNear();
  void restorePatches();
  [[nodiscard]] bool project(std::uint32_t count, std::uint32_t first);
  void projectPlain(std::uint32_t first);
  void projectBlended(std::uint32_t first);
  // A vertex's screen word, or its outcodes when the record straddles the view. True at the end.
  [[nodiscard]] bool keepScreen(std::uint32_t screen);
  void fadeColours();
  void fadeToFar();
  void fadeToFog();
  [[nodiscard]] bool mergeBins();

  // A quad's colour word, corner slots, and its first and fourth corners' colour addresses.
  struct Quad {
    std::uint32_t colourWord = 0;
    std::array<std::uint32_t, 4> slots{};
    std::array<std::uint32_t, 2> colours{};
    bool textured = false;
  };
  [[nodiscard]] std::optional<std::array<std::uint32_t, 4>>
  screens(const std::array<std::uint32_t, 4> &slots, std::uint32_t corners) const;
  void drawQuad(std::uint32_t colourWord);
  void drawWholeQuad(const Quad &quad);
  void drawTriangle(std::uint32_t colourWord);
  void drawSprite(std::uint32_t colourWord);

  std::uint32_t steps_ = 0;      // t2
  std::uint32_t stepsWord_ = 0;  // t3
  std::uint32_t blendSteps_ = 0; // t4
  std::uint32_t cursor_ = 0;     // HI
};

} // namespace spyro1::actors
