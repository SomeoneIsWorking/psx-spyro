#include "render/terrain/guest_terrain_split.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/guest_terrain_polygon.h"

#include <array>
#include <cstddef>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;
using spyro::guest_word::sra;
using spyro::guest_word::signedHalf;
using spyro::guest_word::asWord;

// The two height packings (80025D28): the detail pass's (w << 3) & 0x1FFC and the translucent
// pass's w & 0x3FF, told apart by the header's top bit.
constexpr std::uint32_t kDetailHeightShift = 3;
constexpr std::uint32_t kDetailHeightMask = 0x1FFC;
constexpr std::uint32_t kTranslucentHeightMask = 0x3FF;

constexpr std::uint32_t kAbove = 0x1000;
constexpr std::uint32_t kBelow = 0x2000;
constexpr std::uint32_t kLeft = 0x4000;
constexpr std::uint32_t kRight = 0x8000;

// A grid point as RTPS takes it: 16x scale when every coordinate is within [-0x100, 0xFF].
struct GridVertex {
  std::uint32_t z;
  std::uint32_t xy;
  bool scaled;
};

bool fitsScaled(std::int32_t coordinate) {
  return (((coordinate >> 8) + 1) >> 1) == 0;
}

GridVertex loadGridVertex(const psx::present::EmitMemory &memory, std::uint32_t address) {
  const std::int32_t z = memory.mem_r16s(address + cell::kZ);
  const std::int32_t x = memory.mem_r16s(address + cell::kX);
  const std::int32_t y = memory.mem_r16s(address + cell::kY);
  if (fitsScaled(z) && fitsScaled(x) && fitsScaled(y)) {
    return GridVertex{asWord(z) << 4, (asWord(x) << 4) + (asWord(y) << 20), true};
  }
  return GridVertex{asWord(z), asWord(x) + (asWord(y) << 16), false};
}

} // namespace

SplitSector readSplitHeader(TerrainFrame &frame, std::uint32_t header) {
  Core &core = frame.core;
  const std::uint32_t sector = header << 2;
  const std::uint32_t zx = frame.memory.mem_r32(sector + 0x08);
  SplitSector result;
  if (asSigned(sector) < 0) {
    result.heightShift = kDetailHeightShift;
    result.heightMask = kDetailHeightMask;
  } else {
    result.heightShift = 0;
    result.heightMask = kTranslucentHeightMask;
  }
  core.gte().writeControl(gte::kLight4, frame.memory.mem_r32(sector + 0x10) >> 31);
  const std::uint32_t y = frame.memory.mem_r16(sector + 0x0E);
  result.originZ = (zx >> 14) - core.gte().readControl(gte::kLight0);
  result.originX = core.gte().readControl(gte::kLight1) - ((zx & 0xFFFFu) << 2);
  const std::uint32_t layout = frame.memory.mem_r32(sector + 0x14);
  result.originY = core.gte().readControl(gte::kLight2) - (y << 2);
  result.vertices = sector + 0x1C + ((layout >> 22) & 0x3FCu);
  result.colours = result.vertices + ((layout << 2) & 0x3FCu);
  frame.v0 = layout;
  frame.v1 = y;
  return result;
}

void walkSplitList(TerrainFrame &frame, std::uint32_t list, SplitPolygonPass &pass) {
  SplitSector sector;
  for (;;) {
    std::uint32_t entry = frame.memory.mem_r32(list);
    list += 4;
    if (entry == 0) {
      return;
    }
    if (asSigned(entry) >= 0) {
      sector = pass.sector(entry);
      entry = frame.memory.mem_r32(list);
      list += 4;
    }
    const std::uint32_t bits = entry & 3u;
    pass.polygon(
        sector,
        SplitPolygon{entry ^ bits, (bits & kSplitOverflow) != 0, (bits & kSplitTriangle) != 0});
  }
}

GridPoint
unpackGridVertex(const TerrainFrame &frame, const SplitSector &sector, std::uint32_t index) {
  const std::uint32_t word = frame.memory.mem_r32(sector.vertices + index);
  return GridPoint{((word >> 19) & 0x1FFCu) + sector.originZ,
                   sector.originX - ((word >> 8) & 0x1FFCu),
                   sector.originY - ((word << sector.heightShift) & sector.heightMask)};
}

void storeGridPoint(TerrainFrame &frame, std::uint32_t cellIndex, const GridPoint &point) {
  const std::uint32_t address = kScratchpad + cellIndex * kCellSize;
  frame.memory.mem_w16(address + cell::kZ, static_cast<std::uint16_t>(point.z));
  frame.memory.mem_w16(address + cell::kX, static_cast<std::uint16_t>(point.x));
  frame.memory.mem_w16(address + cell::kY, static_cast<std::uint16_t>(point.y));
}

void loadCornerColours(TerrainFrame &frame, std::uint32_t *colours, std::uint32_t count) {
  const std::uint32_t fog = frame.core.gte().readControl(gte::kLight3);
  if (fog == 0) {
    return;
  }
  frame.core.gte().writeData(gte::kIr0, fog);
  frame.core.gte().writeData(gte::kRgbc, colours[0]);
  if (asSigned(frame.core.gte().readControl(gte::kLight4)) > 0) {
    return;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    if (i != 0) {
      frame.core.gte().writeData(gte::kRgbc, colours[i]);
    }
    frame.core.gte().execute(gte::kDpcs);
    colours[i] = frame.core.gte().readData(gte::kRgb2);
  }
}

void projectGrid(TerrainFrame &frame, std::uint32_t count, bool markBehind) {
  Core &core = frame.core;
  const psx::present::EmitMemory &memory = frame.memory;
  const std::uint32_t end = kScratchpad + (count + 1) * kCellSize;
  // Retail zeroes the cell after the last so its one-ahead read of it is harmless.
  memory.mem_w32(end - kCellSize, 0);
  GridVertex next = loadGridVertex(memory, kScratchpad);
  core.gte().writeData(gte::kVz0, next.z);
  core.gte().writeData(gte::kVxy0, next.xy);
  std::uint32_t address = kScratchpad + kCellSize;
  do {
    core.gte().execute(gte::kRtps);
    const bool scaled = next.scaled;
    next = loadGridVertex(memory, address);
    const psx::gte::ProjectedVertex projected = core.gte().projected(2);
    const std::uint32_t sxy = projected.word();
    std::uint32_t code = core.gte().readData(gte::kSz3);
    core.gte().writeData(gte::kVz0, next.z);
    core.gte().writeData(gte::kVxy0, next.xy);
    address += kCellSize;
    if (scaled) {
      code >>= 4;
    }
    if (markBehind && asSigned(code) <= 0) {
      code |= kOutcodeMask;
    }
    if (frame.bounds.aboveTop(sxy)) {
      code |= kAbove;
    }
    if (frame.bounds.belowBottom(sxy)) {
      code |= kBelow;
    }
    if (frame.bounds.atOrLeftOfLeft(sxy)) {
      code |= kLeft;
    }
    if (frame.bounds.atOrRightOfRight(sxy)) {
      code |= kRight;
    }
    memory.storeVertex(address - 2 * kCellSize + cell::kScreen, projected);
    memory.mem_w32(address - 2 * kCellSize + cell::kDepth, code);
  } while (address != end);
}

void emitCrackTriangle(TerrainFrame &frame, std::uint32_t entry, const CrackPolygon &polygon) {
  const auto [colourAdjust, uv, page, bias] = polygon;
  Core &core = frame.core;
  const psx::present::EmitMemory &memory = frame.memory;
  const std::uint32_t cells = memory.mem_r32(entry);
  const std::uint32_t uvFirst = memory.mem_r32(entry + 4);
  const std::uint32_t uvRest = memory.mem_r32(entry + 8);
  TrianglePiece piece = readPieceDepths<3>(
      memory, {kScratchpad + (cells >> 16), kScratchpad + (cells & 0xFFFFu), kScratchpad + (uvFirst >> 16)});
  frame.v0 = piece.cells[0];
  frame.v1 = piece.cells[1];
  if (piece.outside()) {
    return;
  }
  const std::uint32_t prim = frame.primitive;
  readPieceScreens(memory, piece);
  storePieceScreens(frame, piece.corners());
  storePieceColours(frame, piece.cells);
  memory.mem_w32(prim + packet::kColour0, memory.mem_r32(piece.cells[0] + cell::kColour) - colourAdjust);
  memory.mem_w32(prim + packet::kUv0, (uvFirst & 0xFFFFu) + uv);
  memory.mem_w32(prim + packet::kUv1, static_cast<std::uint32_t>(asSigned(uvRest) >> 16) + page);
  memory.mem_w32(prim + packet::kUv2, (uvRest & 0xFFFFu) + uv);
  frame.v0 = bias;
  frame.v1 = linkTrianglePiece(frame, polygonBin(piece.depthSum(), bias)).head;
}

void drawEdgeCracks(TerrainFrame &frame, CrackEdges edges, const CrackPolygon &polygon, CrackRun run) {
  std::uint32_t entry = edges.table;
  std::uint32_t remaining = edges.edges;
  while (remaining != 0) {
    const std::uint32_t edge = remaining & 0xFFu;
    remaining >>= 8;
    entry += run.perEdge * kCrackEntrySize;
    if (edge == 0) {
      continue;
    }
    const std::uint32_t drawn = (edge & 1u) != 0 ? run.perEdge : run.perEdge - run.drawnWholeOnly;
    const std::uint32_t first = entry - run.perEdge * kCrackEntrySize;
    for (std::uint32_t i = 0; i < drawn; ++i) {
      emitCrackTriangle(frame, first + i * kCrackEntrySize, polygon);
    }
  }
}

std::array<std::uint32_t, 3> trianglePieceCells(std::uint32_t layout) {
  return {kScratchpad + ((layout >> 20) & 0xFF0u), kScratchpad + ((layout >> 12) & 0xFF0u), kScratchpad + ((layout >> 4) & 0xFF0u)};
}

bool pieceFacesCamera(TerrainFrame &frame,
                      const QuadPiece &piece,
                      bool overflow,
                      Sidedness sides,
                      SecondTriangle second) {
  if (!overflow || facesCamera(frame, piece.screens, sides, second)) {
    return true;
  }
  frame.memory.mem_w32(frame.primitive + packet::kXy0, piece.screens[0]);
  return false;
}

bool pieceFacesCamera(TerrainFrame &frame, const TrianglePiece &piece, bool overflow, Sidedness sides) {
  if (!overflow || triangleFacesCamera(frame, piece.screens, sides)) {
    return true;
  }
  frame.memory.mem_w32(frame.primitive + packet::kXy0, piece.screens[0]);
  return false;
}

void storePieceScreens(TerrainFrame &frame, PieceCorners corners) {
  constexpr std::array<std::uint32_t, 4> kSlots = {packet::kXy0, packet::kXy1, packet::kXy2, packet::kXy3};
  for (std::size_t i = 0; i < corners.screens.size(); ++i) {
    frame.memory.storeVertex(frame.primitive + kSlots[i],
                             frame.memory.loadVertex(corners.cells[i] + cell::kScreen, corners.screens[i]));
  }
}

void storePieceColours(TerrainFrame &frame, std::span<const std::uint32_t> cells) {
  constexpr std::array<std::uint32_t, 4> kSlots = {
      packet::kColour0, packet::kColour1, packet::kColour2, packet::kColour3};
  for (std::size_t i = 0; i < cells.size(); ++i) {
    frame.memory.mem_w32(frame.primitive + kSlots[i], frame.memory.mem_r32(cells[i] + cell::kColour));
  }
}

Linked linkQuadPiece(TerrainFrame &frame, std::uint32_t bin) {
  frame.memory.mem_w32(frame.primitive, guest_packet::kGt4Tag);
  return frame.linkAndAdvance(bin, PacketSize{guest_packet::kGt4Bytes});
}

Linked linkTrianglePiece(TerrainFrame &frame, std::uint32_t bin) {
  frame.memory.mem_w32(frame.primitive, guest_packet::kGt3Tag);
  return frame.linkAndAdvance(bin, PacketSize{guest_packet::kGt3Bytes});
}

GpuSpan gpuSpan(std::span<const std::uint32_t> screens) {
  constexpr std::int32_t kGpuSpanHigh = 0x200;
  constexpr std::int32_t kGpuSpanWide = 0x400;
  constexpr std::array<std::array<std::size_t, 2>, 5> kPairs = {
      {{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 3}}};
  const std::size_t pairCount = screens.size() == 4 ? kPairs.size() : 3;
  const auto x = [](std::uint32_t sxy) {
    return asSigned(signedHalf(sxy));
  };
  const auto y = [](std::uint32_t sxy) {
    return asSigned(sra(sxy, 16));
  };
  const auto exceeds = [](std::int32_t span, std::int32_t limit) {
    return span >= limit || span <= -limit;
  };
  for (std::size_t i = 0; i < pairCount; ++i) {
    if (exceeds(y(screens[kPairs[i][0]]) - y(screens[kPairs[i][1]]), kGpuSpanHigh)) {
      return GpuSpan::TooHigh;
    }
  }
  for (std::size_t i = 0; i < pairCount; ++i) {
    if (exceeds(x(screens[kPairs[i][0]]) - x(screens[kPairs[i][1]]), kGpuSpanWide)) {
      return GpuSpan::TooWide;
    }
  }
  return GpuSpan::Within;
}

void pullTowardPair(TerrainFrame &frame, const GridPull &pull) {
  const auto [point, first, second, ownWeight, pairWeight] = pull;
  Core &core = frame.core;
  const psx::present::EmitMemory &memory = frame.memory;
  const auto low = signedHalf;
  const auto high = [](std::uint32_t sxy) {
    return sra(sxy, 16);
  };
  const psx::gte::ProjectedVertex self = memory.loadVertex(point + cell::kScreen);
  const std::uint32_t own = self.word();
  core.gte().writeData(gte::kIr0, ownWeight);
  core.gte().writeData(gte::kIr1, low(own));
  core.gte().writeData(gte::kIr2, high(own));
  const psx::gte::ProjectedVertex pairFirst = memory.loadVertex(first + cell::kScreen);
  const psx::gte::ProjectedVertex pairSecond = memory.loadVertex(second + cell::kScreen);
  const std::uint32_t a = pairFirst.word();
  const std::uint32_t b = pairSecond.word();
  core.gte().execute(gte::kGpf);
  core.gte().writeData(gte::kIr0, pairWeight);
  core.gte().writeData(gte::kIr1, low(a) + low(b));
  core.gte().writeData(gte::kIr2, high(a) + high(b));
  const std::uint32_t ownX = core.gte().readData(gte::kMac1);
  const std::uint32_t ownY = core.gte().readData(gte::kMac2);
  core.gte().execute(gte::kGpf);
  const std::int32_t x = asSigned(ownX + core.gte().readData(gte::kMac1)) >> 8;
  const std::int32_t y = asSigned(ownY + core.gte().readData(gte::kMac2)) >> 8;
  // IR0 holds the weights as signed halfwords.
  const auto selfWeight = static_cast<float>(static_cast<std::int16_t>(ownWeight));
  const auto pairsWeight = static_cast<float>(static_cast<std::int16_t>(pairWeight));
  const auto blend = [selfWeight, pairsWeight](float own, float pair) {
    return (selfWeight * own + pairsWeight * pair) / 256.0f;
  };
  const GtePlace at = self.place();
  const GtePlace pa = pairFirst.place();
  const GtePlace pb = pairSecond.place();
  memory.storeVertex(point + cell::kScreen,
                     {(static_cast<std::uint32_t>(x) & 0xFFFFu) + (static_cast<std::uint32_t>(y) << 16),
                      {blend(at.x, pa.x + pb.x), blend(at.y, pa.y + pb.y)}});
}

std::uint32_t quadNeighbourEdges(TerrainFrame &frame,
                                 std::uint32_t links,
                                 std::uint32_t draw,
                                 std::uint32_t bit) {
  const psx::present::EmitMemory &memory = frame.memory;
  const std::uint32_t flags = frame.scratch;
  const std::uint32_t first = memory.mem_r8(flags + (links >> 19)) & bit;
  const std::uint32_t second = memory.mem_r8(flags + ((links >> 6) & 0x1FFFu)) & bit;
  const std::uint32_t third = memory.mem_r8(flags + (links & 0x3Fu) + ((draw >> 6) & 0x1FC0u)) & bit;
  const std::uint32_t fourth = memory.mem_r8(flags + (draw >> 19)) & bit;
  return first + (second << 8) + (third << 16) + (fourth << 24);
}

std::uint32_t triangleNeighbourEdges(TerrainFrame &frame,
                                     std::uint32_t links,
                                     std::uint32_t colourIndices,
                                     std::uint32_t bit) {
  const psx::present::EmitMemory &memory = frame.memory;
  const std::uint32_t flags = frame.scratch;
  const std::uint32_t first = memory.mem_r8(flags + (links >> 19)) & bit;
  const std::uint32_t second = memory.mem_r8(flags + ((links >> 6) & 0x1FFFu)) & bit;
  const std::uint32_t third =
      memory.mem_r8(flags + ((colourIndices & 0xFEu) << 5) + (links & 0x3Fu)) & bit;
  return first + (second << 8) + (third << 16);
}

} // namespace spyro::guest_terrain
