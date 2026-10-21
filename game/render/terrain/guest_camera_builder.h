// The guest's own camera builders (SCUS_944.25 FUN_8001c2f8, SCUS_942.28 0x80033C50), re-run so an
// in-between camera can interpolate.

// Each is the only writer of its image's view matrix (0x80067E98 / 0x80076DE4) and projection
// matrix (0x80067E84 / 0x80076DD0), which is the same matrix with row 1 scaled by 0x140 / 512: the
// display's vertical scale. Only the input angles are interpolated; the packed matrices are scaled,
// shifted and truncated products of those angles, so they do not move linearly with them.

// The tables are the guest's and stay the guest's: `Builder` reads them through `Core` and never
// writes them. They are 320 signed entries wide, not 256, because the last interpolation reads the
// entry after the last one.

#pragma once

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"

#include <array>
#include <cstdint>

namespace spyro::guest_camera {

// FUN_8001b61C and FUN_8001b664 both index `(angle & 0xFFF) >> 4` with the low nibble as the
// interpolation fraction, so 4096 is the period.
inline constexpr unsigned kTurnBits = 12;
// 0x140 >> 9 is 5/8, the display's vertical scale.
inline constexpr std::int32_t kVerticalScaleShift = 9;
inline constexpr std::int32_t kVerticalScale = 0x140;

// Three signed 16-bit angles read from 0x80067EC8, the low half of three consecutive words.
struct Angles {
  std::int32_t aroundX = 0; // the first angle halfword; Rx is the left factor
  std::int32_t aroundY = 0;
  std::int32_t aroundZ = 0;
};

// Both matrices the builder writes, in the GTE's own control-register packing.
struct Matrices {
  std::array<std::uint32_t, psx::gte::kRotationWords> projection{};
  std::array<std::uint32_t, psx::gte::kRotationWords> view{};
};

// FUN_8001b61C / FUN_8001b664 over one table, read one entry at a time and never written: the
// guest's own arithmetic and truncating shift, since a fraction rounded differently is a different
// angle and a different matrix.
class AngleTable {
public:
  AngleTable(Core &core, std::uint32_t base) : core_(core), base_(base) {}

  [[nodiscard]] std::int32_t at(std::uint32_t angle) const;

private:
  [[nodiscard]] std::int32_t entry(std::uint32_t index) const;

  Core &core_;
  std::uint32_t base_;
};

// A 3x3 rotation of 12-bit fractions, row-major.
struct Rotation {
  std::int32_t row[3][3]{};
};

// `a * b` as one GTE MVMVA per column of `b`: products shifted by 12 and saturated to int16, as
// libgte's MulMatrix and the builders' own product leave them.
[[nodiscard]] Rotation multiply(const Rotation &a, const Rotation &b);

// One image's camera builder: three rotations from the angle tables, multiplied in the image's
// order, then row 1 scaled by 0x140 / 512 for the drawer's copy with the image's rounding.
class Builder {
public:
  // `sineTable` is the image's own (SCUS_944.25 0x80061BD8); cosine reads it kCosineOffset on.
  Builder(Core &core, std::uint32_t sineTable)
      : sin_(core, sineTable), cos_(core, sineTable + guest_render_globals::kCosineOffset) {}
  virtual ~Builder() = default;
  Builder(const Builder &) = delete;
  Builder &operator=(const Builder &) = delete;

  [[nodiscard]] std::int32_t sin(std::uint32_t angle) const {
    return sin_.at(angle);
  }
  [[nodiscard]] std::int32_t cos(std::uint32_t angle) const {
    return cos_.at(angle);
  }

  [[nodiscard]] Matrices build(const Angles &angles) const;

private:
  // Rx, Ry and Rz into the view matrix.
  [[nodiscard]] virtual Rotation
  compose(const Rotation &x, const Rotation &y, const Rotation &z) const = 0;
  // One row-1 element times 0x140, divided by 512.
  [[nodiscard]] virtual std::int32_t scaleVertical(std::int32_t element) const = 0;

  AngleTable sin_;
  AngleTable cos_;
};

// SCUS_944.25 FUN_8001c2f8: Rx * (Ry * Rz), the scale shifted (rounding toward -infinity).
class EngineBuilder final : public Builder {
public:
  using Builder::Builder;

private:
  [[nodiscard]] Rotation
  compose(const Rotation &x, const Rotation &y, const Rotation &z) const override {
    return multiply(x, multiply(y, z));
  }
  [[nodiscard]] std::int32_t scaleVertical(std::int32_t element) const override {
    return (element * kVerticalScale) >> kVerticalScaleShift;
  }
};

// At t == 0 and t == 1 the result is the operand's angles bit for bit, so an in-between at either
// end of its interval is the guest's own camera.
Angles interpolate(const Angles &from, const Angles &to, float t);

} // namespace spyro::guest_camera