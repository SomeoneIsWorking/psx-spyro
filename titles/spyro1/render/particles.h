// particles.h — SCUS_942.28's RasterizeEmitList 0x800573C8: the 0x20-byte particle records
// from [g_pEmitListBase], each drawn by its kind as a textured quad, a turned quad, a line or a dot
// and linked alone into its depth bin. Retail clears a record's seen byte (+3) and sets it when the
// record passes the view tests; the byte follows retail's 4:3 window even when the draw is widened.
// Runs over guest memory for the real field and over host memory for an in-between, one record at
// a time.
#pragma once

#include "guest_memory_map.h"

#include "projected_vertex.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_screen.h"

#include <array>
#include <cstdint>
#include <span>

class Core;

namespace spyro1::particles {

// g_pEmitListBase: the first record. A record whose kind halfword is 0xFFxx ends the list.
inline constexpr std::uint32_t kListBase = 0x80075824u;
inline constexpr std::uint32_t kRecordBytes = 0x20u;
// Per texture group, a table of 8-byte frames whose +4 and +8 words are a quad's UV words.
inline constexpr std::uint32_t kTextureGroups = 0x80076278u;
// Where the prologue spills the borrowed registers.
inline constexpr std::uint32_t kRegisterSpill = psx::kScratchpadPhysicalBase;

namespace record {
inline constexpr std::uint32_t kKind = 0x00; // halfword: kind << 8 | texture group
inline constexpr std::uint32_t kSeen = 0x03;
inline constexpr std::uint32_t kPosition = 0x04; // x, y, z halfwords
inline constexpr std::uint32_t kAngle = 0x0B;    // turned kinds: 256 steps a turn
inline constexpr std::uint32_t kEnd = 0x0A;      // lines: the second point's x, y, z halfwords
} // namespace record

enum class Outcome : std::uint8_t {
  Drawn,
  Skipped, // culled, behind its depth bias, or a negative kind that is not the end
  End,
  PoolFull,
};

// A packet's bytes: one-pixel line, line, textured quad.
enum class Packet : std::uint8_t {
  Dot = 0x10,
  Line = 0x14,
  Quad = 0x28,
};

class EmitList {
public:
  EmitList(Core &core,
           const psx::present::EmitMemory &memory,
           const spyro::guest_terrain::ScreenBounds &bounds);

  // 800573FC..800574BC: the camera into the GTE, the pool limit, cursor and table read.
  void begin();
  // 800574C0..80058098: the record at `at`.
  Outcome draw(std::uint32_t at);
  // 8005809C..800580B4: the pool flag when it ran out, the cursor stored and $v0/$v1 left.
  void end(bool poolFull);

private:
  Outcome quad(std::uint32_t at);
  Outcome placedTurnedQuad(std::uint32_t at);
  Outcome placedLine(std::uint32_t at);
  Outcome scaledQuad(std::uint32_t at);
  Outcome turnedQuad(std::uint32_t at);
  Outcome line(std::uint32_t at);
  Outcome dot(std::uint32_t at);
  // 800577D0..80057950: the turned square around the translation, its UVs, the camera back.
  struct TurnedQuad {
    std::uint32_t frames = 0; // the texture group's frame table
    std::uint32_t depth = 0;  // the bin before the record's bias
  };
  Outcome turnedTail(std::uint32_t at, TurnedQuad quad);
  // A frame's two UV words as an FT4's four.
  void writeUvs(std::uint32_t first, std::uint32_t second);
  // The packet at the cursor linked alone into the bin `binOffset / 8`; returns the slot.
  std::uint32_t link(std::uint32_t binOffset, Packet packet);
  [[nodiscard]] std::uint32_t textureGroup() const;
  // The seen byte as retail's 512 columns decide it, whatever the widened window drew.
  void markSeen(std::uint32_t at, bool retailDropped);
  [[nodiscard]] bool leftOf(std::uint32_t sxy) const;
  [[nodiscard]] bool rightOf(std::uint32_t sxy) const;

  Core &core_;
  const psx::present::EmitMemory &memory_;
  std::uint32_t leftEdge_ = 0;   // the left bound << 16
  std::uint32_t rightEdge_ = 0;  // the right bound << 16, retail's $s6
  std::uint32_t camera_[3] = {}; // $s7, $t8, $t9: the camera position / 4
  std::uint32_t limit_ = 0;      // $t7
  std::uint32_t cursor_ = 0;     // $fp
  std::uint32_t table_ = 0;      // $sp
  std::uint32_t kind_ = 0;       // $gp
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
};

} // namespace spyro1::particles
