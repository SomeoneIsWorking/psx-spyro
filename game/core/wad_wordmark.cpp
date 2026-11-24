#include "core/wad_wordmark.h"

#include "disc_file.h"

#include <lucent/log.h>

#include <cstring>
#include <utility>
#include <vector>

namespace spyro {
namespace {

constexpr const char *kWad = "/WAD.WAD";
// The directory's (offset, length) word pairs, one per entry.
constexpr std::uint32_t kDirectoryPairBytes = 8;
constexpr int kBlockWidth = 512;
constexpr std::uint32_t kRowBytes = kBlockWidth * 2;

} // namespace

std::optional<psx::present::ArtScene> wadWordmark(psx::cd::Disc &disc,
                                                  const WadWordmark &wordmark) {
  std::vector<std::uint8_t> pair;
  if (!psx::cd::readDiscFileRange(disc,
                                  kWad,
                                  static_cast<std::uint32_t>(wordmark.entry) * kDirectoryPairBytes,
                                  kDirectoryPairBytes,
                                  pair)) {
    lucent::warn("picker", "{} has no directory entry {}", kWad, wordmark.entry);
    return std::nullopt;
  }
  std::uint32_t entryOffset = 0;
  std::uint32_t entryLength = 0;
  std::memcpy(&entryOffset, pair.data(), sizeof entryOffset);
  std::memcpy(&entryLength, pair.data() + 4, sizeof entryLength);
  const std::uint32_t blockBytes = static_cast<std::uint32_t>(wordmark.rows) * kRowBytes;
  std::vector<std::uint8_t> block;
  if (wordmark.blockOffset + blockBytes > entryLength ||
      !psx::cd::readDiscFileRange(
          disc, kWad, entryOffset + wordmark.blockOffset, blockBytes, block)) {
    lucent::warn("picker", "{} entry {} does not hold its wordmark block", kWad, wordmark.entry);
    return std::nullopt;
  }
  psx::present::ArtUpload upload{
      .rect = {wordmark.origin.x, wordmark.origin.y, kBlockWidth, wordmark.rows},
      .words = std::vector<std::uint16_t>(block.size() / 2)};
  std::memcpy(upload.words.data(), block.data(), block.size());
  psx::present::ArtScene scene{.canvas = {0, 0, wordmark.sprite.width, wordmark.sprite.height}};
  scene.uploads.push_back(std::move(upload));
  scene.sprites.push_back(wordmark.sprite);
  return scene;
}

} // namespace spyro
