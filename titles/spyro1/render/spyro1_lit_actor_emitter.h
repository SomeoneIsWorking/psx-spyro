// spyro1_lit_actor_emitter.h — EmitSecondaryActorPrimitives 0x80020F34: the regular emitter with
// each vertex's view position kept beside its depth, and the entry's secondary word lighting the
// triangles flagged lit: a flat light from the face normal, or a reflection whose uvs come from
// each vertex's direction off the record's centre.
#pragma once

#include "spyro1_actor_emitter.h"

#include <cstdint>

namespace spyro1::actors {

class LitActorEmitter final : public ActorEmitter {
public:
  using ActorEmitter::ActorEmitter;

private:
  [[nodiscard]] std::uint32_t depthTable(std::uint32_t count) const override;
  void keepDepth(const Projected &vertex) override;
  void drawPolygons() override;
  [[nodiscard]] QuadFaces faceQuad(bool twoSided, std::uint32_t fourthScreen) override;
  [[nodiscard]] std::uint32_t depthAt(std::uint32_t slot) const override;
  void drawTriangleTail(const Triangle &triangle) override;

  // A corner slot's 8-byte vertex record: depth, then the view position's three halfwords.
  [[nodiscard]] std::uint32_t vertexAt(std::uint32_t slot) const;
  void aimReflection();
  [[nodiscard]] TriangleColours flatColours(const Triangle &triangle);
  void drawReflection(const Triangle &triangle, const TriangleColours &colours);
  [[nodiscard]] std::uint32_t reflectionUv(std::uint32_t vertex);
  [[nodiscard]] std::uint32_t directionUv(std::uint32_t x, std::uint32_t y, std::uint32_t z);

  std::uint32_t light_ = 0; // HI: the secondary word
};

} // namespace spyro1::actors
