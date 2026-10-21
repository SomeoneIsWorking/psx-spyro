// glows.h — SCUS_942.28's glows, both drawn by DrawWorldParticleEmitters 0x80058BA8:
//   0x800580F4  the sixteen glow emitters at kEmitters, each a fan of additive triangles around a
//               projected centre, behind one draw mode, in the centre's depth bin;
//   0x800584C4  the eight sparkles at kSparkles, each a two-line star that turns and fades by the
//               field's delta, killed when it leaves retail's 4:3 view.
// Both run over guest memory for the real field and over host memory for an in-between, one
// record at a time.
#pragma once

#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_screen.h"

#include <cstdint>
#include <optional>

class Core;

namespace psx::present {
class PrimitiveSink;
}

namespace spyro1::glows {

inline constexpr std::uint32_t kEmitters = 0x80078800u;
inline constexpr std::uint32_t kEmitterBytes = 0x24u;
inline constexpr std::uint32_t kEmitterCount = 16;
// An emitter: rim point count, rim offset table, position pointer, colour, size, offset from the
// position, depth bias.
namespace emitter {
inline constexpr std::uint32_t kCount = 0x00;
inline constexpr std::uint32_t kRim = 0x04;
inline constexpr std::uint32_t kPosition = 0x08;
inline constexpr std::uint32_t kColour = 0x0C;
inline constexpr std::uint32_t kSize = 0x10;
inline constexpr std::uint32_t kOffset = 0x14;
inline constexpr std::uint32_t kDepthBias = 0x20;
} // namespace emitter

inline constexpr std::uint32_t kSparkles = 0x80077108u; // g_Sparkles
inline constexpr std::uint32_t kSparkleBytes = 0x18u;
inline constexpr std::uint32_t kSparkleCount = 8;
// A sparkle: position, then fade, full fade, angle and signed turn bytes, the line packet's
// command word, and the size (low byte) and farthest depth (second byte).
namespace sparkle {
inline constexpr std::uint32_t kFade = 0x0C;
inline constexpr std::uint32_t kAngle = 0x0E;
inline constexpr std::uint32_t kCommand = 0x10;
inline constexpr std::uint32_t kSizeDepth = 0x14;
} // namespace sparkle

class GlowFans {
public:
  GlowFans(Core &core,
           const psx::present::EmitMemory &memory,
           const spyro::guest_terrain::ScreenBounds &bounds,
           psx::present::PrimitiveSink *places = nullptr);

  // 800580F4..80058190: the camera into the GTE and the cursor and table read.
  void begin();
  // 80058198..80058464: one emitter. The depth bin its packets went to, nullopt when it drew
  // none.
  std::optional<std::uint32_t> draw(std::uint32_t emitter);
  // 80058474..800584C0: the cursor stored and $v0/$v1 left.
  void end();

private:
  struct Point {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
  };
  [[nodiscard]] std::uint32_t outcode(Point point) const;
  void link(std::uint32_t slot, std::uint32_t packet);

  Core &core_;
  const psx::present::EmitMemory &memory_;
  spyro::guest_terrain::ScreenBounds bounds_;
  psx::present::PrimitiveSink *places_;
  std::uint32_t camera_[3] = {};
  std::uint32_t table_ = 0;
  std::uint32_t cursor_ = 0;
};

class Sparkles {
public:
  Sparkles(Core &core,
           const psx::present::EmitMemory &memory,
           const spyro::guest_terrain::ScreenBounds &bounds,
           psx::present::PrimitiveSink *places = nullptr);

  // 800584C4..80058550: the camera into the rotation and light matrices, `delta` taken.
  void begin(std::uint32_t delta);
  // 80058560..80058818: one sparkle aged and drawn. The depth bin of its lines, nullopt when it
  // drew none.
  std::optional<std::uint32_t> draw(std::uint32_t record);
  // The fade zeroed when retail's 4:3 window would have killed the sparkle just drawn in a margin;
  // apart for the producer to save what was drawn first.
  void retire(std::uint32_t record);
  // 8005881C: the cursor stored and $v0/$v1 left.
  void end();

private:
  Core &core_;
  const psx::present::EmitMemory &memory_;
  spyro::guest_terrain::ScreenBounds bounds_;
  psx::present::PrimitiveSink *places_ = nullptr;
  bool retiring_ = false;
  std::uint32_t delta_ = 0;
  std::uint32_t camera_[3] = {};
  std::uint32_t table_ = 0;
  std::uint32_t cursor_ = 0;
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
};

} // namespace spyro1::glows
