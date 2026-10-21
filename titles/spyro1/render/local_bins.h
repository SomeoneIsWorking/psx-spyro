// local_bins.h — the local depth bins SCUS_942.28's actor passes draw one record into: each
// packet at the cursor linked into a bin, the lowest and highest bins reached, and the merge of the
// bins into the ordering table (the emitters 0x8001F798/0x80020F34 and RasterizePairedActor
// 0x80023AC4; the sprite queue chains them itself).
#pragma once

#include "render/field/guest_working_memory.h"

#include <cstdint>

namespace spyro1::actors {

class LocalBins {
public:
  explicit LocalBins(const psx::present::EmitMemory &memory) : memory_(memory) {}

  // Where the bins start and packets go.
  struct Start {
    std::uint32_t base = 0;
    std::uint32_t cursor = 0;
  };
  // No bin reached yet.
  void begin(const Start &start);

  // Links the packet at the cursor into the bin at `slot` and steps the cursor past it (its length
  // from its tag). Returns retail's register after the link: the bin's old head, or the highest bin
  // less `slot`.
  std::uint32_t link(std::uint32_t slot);

  // Steps the cursor past the packet at it, which is linked elsewhere; returns the packet.
  std::uint32_t take();

  [[nodiscard]] bool empty() const;

  // What the merge leaves in $v0 and $v1: the table base and the local bytes per table bin.
  struct Merged {
    std::uint32_t table = 0;
    std::uint32_t stride = 0;
  };
  // The ordering table, 0x100 >> `shift` local bytes to a table bin, and the record's table bin.
  struct Target {
    std::uint32_t shift = 0;
    std::uint32_t depthBin = 0;
    std::uint32_t table = 0;
  };
  // The bins, highest first, into the table, the first from the depth bin back by the record's
  // depth span; each emptied.
  Merged merge(const Target &target);

  [[nodiscard]] std::uint32_t cursor() const {
    return cursor_;
  }
  [[nodiscard]] std::uint32_t lowest() const {
    return lowest_;
  }
  [[nodiscard]] std::uint32_t highest() const {
    return highest_;
  }

private:
  const psx::present::EmitMemory &memory_;
  std::uint32_t cursor_ = 0;
  std::uint32_t lowest_ = 0;
  std::uint32_t highest_ = 0;
};

} // namespace spyro1::actors
