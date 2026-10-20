#include "core/title_logo.h"

#include "core.h"
#include "game.h"
#include "gpu_device.h"
#include "vram_pixel.h"

#include <lucent/log.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace spyro {
namespace {

// Bounds a misread sprite record so it cannot ask for a snapshot the size of VRAM.
constexpr int kLogoMaximumPixels = 1 << 20;

// One 8-byte sprite record: tpage, clut, w, h, u, v. The guest's emitter loads the two halfwords
// from the start and the four bytes from the third word; Spyro 1's first two records agree —
// 0x8006FACC holds 0x7FE00098.
struct SpriteRecord {
  std::uint16_t tpage = 0;
  std::uint16_t clut = 0;
  std::uint32_t w = 0;
  std::uint32_t h = 0;
  std::uint32_t u = 0;
  std::uint32_t v = 0;
};

SpriteRecord readSpriteRecord(Core &core, std::uint32_t address) {
  return SpriteRecord{core.mem_r16(address),
                      core.mem_r16(address + 2),
                      core.mem_r8(address + 4),
                      core.mem_r8(address + 5),
                      core.mem_r8(address + 6),
                      core.mem_r8(address + 7)};
}

} // namespace

// The guest keeps a pointer global; the overlay's state block hangs off it and its tick is what
// retail tests to decide its logo is up. A null pointer means the title has not reached its
// overlay, which is not a failure.
bool titleLogoGateOpen(Core &core, const TitleLogoFacts &facts) {
  if (!facts.valid || facts.gate.pointerGlobal == 0) {
    return false;
  }
  const std::uint32_t block = core.mem_r32(facts.gate.pointerGlobal);
  if (block == 0) {
    return false;
  }
  return core.mem_r32(block + facts.gate.tickOffset) >= facts.gate.tickValue;
}

std::optional<psx::host::LogoImage> extractPanelLogo(Core &core, const TitleLogoFacts &facts) {
  if (!facts.valid) {
    return std::nullopt;
  }
  if (!titleLogoGateOpen(core, facts)) {
    // The normal answer for all but the moment this title's screen is up; the caller asks again.
    return std::nullopt;
  }
  if (core.game == nullptr || facts.parts.empty() || facts.parts.size() > kTitleLogoMaximumParts) {
    lucent::warn(
        "picker", "the title's logo names {} part(s) — the panel shows none", facts.parts.size());
    return std::nullopt;
  }

  const std::span<const std::uint16_t> vram = core.gpuDevice().vram();
  // Each part's (u, v) is its recorded position on the page the game uploaded, so the wordmark is
  // placed where the game placed it and is never re-lettered as equal pieces.
  std::vector<SpriteRecord> parts;
  parts.reserve(facts.parts.size());
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  for (const TitleLogoPart &part : facts.parts) {
    const SpriteRecord sprite = readSpriteRecord(core, part.record);
    if (sprite.w == 0 || sprite.h == 0) {
      lucent::debug("picker",
                    "the logo's sprite record at 0x{:X} is empty — the panel shows no logo",
                    part.record);
      return std::nullopt;
    }
    const int x = static_cast<int>(sprite.u);
    const int y = static_cast<int>(sprite.v);
    if (parts.empty()) {
      left = x;
      top = y;
    }
    left = std::min(left, x);
    top = std::min(top, y);
    right = std::max(right, x + static_cast<int>(sprite.w));
    bottom = std::max(bottom, y + static_cast<int>(sprite.h));
    parts.push_back(sprite);
  }
  const int width = right - left;
  const int height = bottom - top;
  if (width <= 0 || height <= 0 || width * height > kLogoMaximumPixels) {
    lucent::error("picker",
                  "the logo's parts cover {}x{} — not a wordmark — refusing to snapshot them",
                  width,
                  height);
    return std::nullopt;
  }

  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 0);
  std::size_t visible = 0;
  for (const SpriteRecord &sprite : parts) {
    const psx::gpu::TexPage page = psx::gpu::decodeTexPage(sprite.tpage);
    if (page.depth == psx::gpu::TexDepth::Reserved) {
      lucent::error("picker",
                    "the logo's texture page {:04X} names depth 3 — no such format — the panel shows no logo",
                    sprite.tpage);
      return std::nullopt;
    }
    const psx::gpu::VramPoint clut = psx::gpu::decodeClut(sprite.clut);
    for (std::uint32_t row = 0; row < sprite.h; ++row) {
      for (std::uint32_t col = 0; col < sprite.w; ++col) {
        const int x = static_cast<int>(sprite.u) + static_cast<int>(col) - left;
        const int y = static_cast<int>(sprite.v) + static_cast<int>(row) - top;
        if (x < 0 || x >= width || y < 0 || y >= height) {
          continue;
        }
        const std::size_t at = static_cast<std::size_t>(y) * width + x;
        std::uint8_t *pixel = &rgba[at * 4];
        const std::uint16_t colour = psx::gpu::sampleTexel(vram,
                                                           page,
                                                           clut,
                                                           static_cast<int>(sprite.u + col),
                                                           static_cast<int>(sprite.v + row));
        // A 0x0000 texel is not drawn: index 0 is the surround and lets the panel through.
        if (colour == 0) {
          pixel[3] = 0;
          continue;
        }
        const psx::gpu::VramPixel texel = psx::gpu::decodeVramPixel(colour);
        pixel[0] = static_cast<std::uint8_t>(psx::gpu::scale5to8(texel.red));
        pixel[1] = static_cast<std::uint8_t>(psx::gpu::scale5to8(texel.green));
        pixel[2] = static_cast<std::uint8_t>(psx::gpu::scale5to8(texel.blue));
        pixel[3] = 255;
        ++visible;
      }
    }
  }

  // VRAM is readable whatever is in it, so only the picture can tell a wordmark from a rectangle
  // of nothing — and an empty panel would be cached for its whole life.
  const std::size_t pixels = static_cast<std::size_t>(width) * height;
  if (visible * 50 < pixels) {
    lucent::debug("picker",
                  "the logo's snapshot holds no artwork ({} of {} texels) — the panel shows none",
                  visible,
                  pixels);
    return std::nullopt;
  }
  lucent::info("picker",
               "the title's own logo snapshotted from its title screen: {}x{} over {} part(s), "
               "first part tpage {:04X} clut {:04X}, index 0 transparent ({} of {} texels)",
               width,
               height,
               parts.size(),
               parts.front().tpage,
               parts.front().clut,
               visible,
               pixels);
  return psx::host::LogoImage{width, height, std::move(rgba)};
}

} // namespace spyro