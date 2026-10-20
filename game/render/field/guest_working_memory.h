// guest_working_memory.h — where a hand-written render routine reads and writes, so the same
// traversal runs over the guest's RAM for a real field and over host bytes for an in-between.
//
// A routine owns working ranges for one field (scratchpad, scratch block, packet pool, ordering
// table) and reads everything else as level data. `GuestMemory` forwards to `Core` and records
// each level word as first read; `HostMemory` maps the working ranges on host bytes, reads level
// words from that record or the guest, and refuses a write it does not own.
#pragma once

#include "core.h"
#include "guest_memory_map.h"
#include "lucent/log.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace spyro::guest_memory {

// [base, base + bytes) of guest address space.
struct GuestRange {
  std::uint32_t base = 0;
  std::uint32_t bytes = 0;

  [[nodiscard]] bool contains(std::uint32_t address) const {
    return address - base < bytes;
  }
};

// Where an in-between links its packets. The address has to be guest-shaped because a packet's
// chain link is a 24-bit main-RAM offset. `bytes == 0` is the refusal case: the caller must not
// present a truncated picture.
struct PacketArena {
  std::uint32_t base = 0;
  std::uint32_t bytes = 0;
};

// Everything from the page above `frameTop`, the end of the last range the traversal owns, to the
// end of main RAM.
[[nodiscard]] constexpr PacketArena packetArenaWindow(std::uint32_t frameTop) {
  const std::uint32_t base = (frameTop + 0xFFFu) & ~0xFFFu;
  if (base >= psx::kMainRamEnd) {
    return PacketArena{};
  }
  return PacketArena{base, psx::kMainRamEnd - base};
}

// A guest word as a real field first read it.
struct LevelWord {
  std::uint32_t address = 0;
  std::uint32_t value = 0;
};

// The working-memory accessor every pass reads and writes through. Three widths, because the
// traversal packs GP0 tag address fields, 16-bit screen words and 8-bit flags into the same ranges.
class WorkingMemory {
public:
  virtual ~WorkingMemory() = default;

  [[nodiscard]] virtual std::uint32_t r32(std::uint32_t address) const = 0;
  [[nodiscard]] virtual std::uint16_t r16(std::uint32_t address) const = 0;
  [[nodiscard]] virtual std::uint8_t r8(std::uint32_t address) const = 0;
  virtual void w32(std::uint32_t address, std::uint32_t value) = 0;
  virtual void w16(std::uint32_t address, std::uint16_t value) = 0;
  virtual void w8(std::uint32_t address, std::uint8_t value) = 0;

  // Whether `address` is one this accessor owns, or whether the read would fall through / the
  // write would be refused. A pass never needs this today; the diagnostics and the tests do.
  [[nodiscard]] virtual bool mapped(std::uint32_t address) const = 0;
};

// The guest's own RAM, byte for byte. What the real field runs over. Reads outside `working`, the
// ranges the traversal owns, are level words and are kept for the field's in-betweens.
class GuestMemory final : public WorkingMemory {
public:
  GuestMemory(Core &core, std::span<const GuestRange> working) : core_(core), working_(working) {}

  [[nodiscard]] std::uint32_t r32(std::uint32_t address) const override {
    recordRead(address);
    return core_.mem_r32(address);
  }
  [[nodiscard]] std::uint16_t r16(std::uint32_t address) const override {
    recordRead(address);
    return core_.mem_r16(address);
  }
  [[nodiscard]] std::uint8_t r8(std::uint32_t address) const override {
    recordRead(address);
    return core_.mem_r8(address);
  }
  void w32(std::uint32_t address, std::uint32_t value) override {
    core_.mem_w32(address, value);
  }
  void w16(std::uint32_t address, std::uint16_t value) override {
    core_.mem_w16(address, value);
  }
  void w8(std::uint32_t address, std::uint8_t value) override {
    core_.mem_w8(address, value);
  }
  // Everything the guest owns, which is the whole address space this accessor is handed.
  [[nodiscard]] bool mapped(std::uint32_t) const override {
    return true;
  }

  // Every level word read so far, with the value it held when first read.
  [[nodiscard]] std::vector<LevelWord> firstReads() const;
  // Starts a new record of first reads, for a routine that saves one per object it draws.
  void forgetReads() {
    firstReads_.clear();
  }

private:
  void recordRead(std::uint32_t address) const {
    const std::uint32_t word = address & ~3u;
    for (const GuestRange &range : working_) {
      if (GuestRange{psx::kseg0Address(range.base), range.bytes}.contains(psx::kseg0Address(word))) {
        return;
      }
    }
    firstReads_.try_emplace(word, core_.mem_r32(word));
  }

  Core &core_;
  std::span<const GuestRange> working_;
  mutable std::unordered_map<std::uint32_t, std::uint32_t> firstReads_;
};

// Guest-shaped ranges backed by host bytes. A main-RAM address is one store under all its names, so
// a byte written through KUSEG reads back through KSEG0.
//
// A mapping copies the guest's current bytes, so a range that is only partly rewritten by a pass
// still holds the level data the passes read out of it. Overlapping mappings MERGE: the primitive
// arena's top is inside the scratch block, and the ordering table's low bins are inside it too, so
// treating them as separate objects would make one of the two lose the other's writes.
class HostMemory final : public WorkingMemory {
public:
  explicit HostMemory(Core &core) : core_(core) {}

  // Own [base, base + bytes) of host storage, seeded from the guest's bytes at the address it
  // already owns and zero elsewhere. A zero `bytes` reserves nothing.
  void map(std::uint32_t base, std::uint32_t bytes);

  // Empty [base, base + bytes) instead of seeding it from the guest's bytes. A range the traversal
  // must find EMPTY is not a range it also reads level data out of, and `map`'s seeding would leave
  // the guest's last field's own contents there for it to adopt — which for an ordering table means
  // linking a fresh packet behind a stale head and drawing a chain the guest never built.
  //
  // This is about the HOST's copy only. The guest's bytes are not touched, which is the whole
  // reason the in-between can have a table at all.
  void mapEmpty(std::uint32_t base, std::uint32_t bytes);

  // Read these words in place of the guest's wherever no mapping owns them.
  void overlay(std::span<const LevelWord> words);

  // Forget every mapping, overlay and counted read and write.
  void clear();

  // From now on keep each level word a read falls through to, with the value it gave: what a run
  // over host memory read of the level, for a field's saved draw.
  void keepLevelReads() {
    keepsLevelReads_ = true;
  }
  [[nodiscard]] std::vector<LevelWord> levelReads() const;

  [[nodiscard]] std::uint32_t r32(std::uint32_t address) const override;
  [[nodiscard]] std::uint16_t r16(std::uint32_t address) const override;
  [[nodiscard]] std::uint8_t r8(std::uint32_t address) const override;
  void w32(std::uint32_t address, std::uint32_t value) override;
  void w16(std::uint32_t address, std::uint16_t value) override;
  void w8(std::uint32_t address, std::uint8_t value) override;
  [[nodiscard]] bool mapped(std::uint32_t address) const override;

  // How many bytes the mappings own in total, and how many reads and writes fell through to the
  // guest or were refused. The write count is the number the run logs, because a non-zero count is
  // a claim about a guarantee this class makes.
  [[nodiscard]] std::uint32_t mappedBytes() const;
  [[nodiscard]] std::uint64_t guestReads() const {
    return guestReads_;
  }
  [[nodiscard]] std::uint64_t refusedWrites() const {
    return refusedWrites_;
  }

  // The merged ranges, in address order.
  [[nodiscard]] std::vector<GuestRange> ranges() const;

private:
  struct Segment {
    std::uint32_t base = 0;
    std::vector<std::uint8_t> bytes;
  };

  // Where a mapping's bytes that no earlier mapping held come from.
  enum class Seed : std::uint8_t { Guest, Zero };

  // Merges `range` (canonical) with every segment it covers or touches.
  void place(GuestRange range, Seed seed);

  // The segment holding canonical `address`, or nullptr.
  [[nodiscard]] const Segment *find(std::uint32_t address) const;
  // The host byte any name of `address` maps to, or nullptr.
  [[nodiscard]] const std::uint8_t *byteAt(std::uint32_t address) const;
  [[nodiscard]] std::uint8_t *byteAt(std::uint32_t address);
  void store(std::uint32_t address, const void *bytes, std::uint32_t count);
  [[nodiscard]] std::uint8_t guestByte(std::uint32_t address) const;
  void refuseWrite(std::uint32_t address, std::uint32_t bytes) const;

  // The overlaid word at a 4-aligned address, or nullptr.
  [[nodiscard]] const std::uint32_t *overlaid(std::uint32_t address) const;

  void noteLevelRead(std::uint32_t address) const;

  Core &core_;
  std::vector<Segment> segments_;
  std::unordered_map<std::uint32_t, std::uint32_t> overlay_;
  bool keepsLevelReads_ = false;
  mutable std::unordered_map<std::uint32_t, std::uint32_t> levelReads_;
  mutable std::uint64_t guestReads_ = 0;
  mutable std::uint64_t refusedWrites_ = 0;
};

// A saved draw: a trivially copyable header, then the level words the draw read.
template <typename Header> struct SavedDraw {
  Header header;
  std::vector<LevelWord> reads;
};

template <typename Header>
[[nodiscard]] std::vector<std::byte> packDraw(const Header &header,
                                              std::span<const LevelWord> reads) {
  static_assert(std::is_trivially_copyable_v<Header>);
  std::vector<std::byte> bytes(sizeof(Header) + reads.size() * sizeof(LevelWord));
  std::memcpy(bytes.data(), &header, sizeof(Header));
  std::memcpy(bytes.data() + sizeof(Header), reads.data(), reads.size() * sizeof(LevelWord));
  return bytes;
}

// Aborts naming `what` when `bytes` is not a header and whole words.
template <typename Header>
[[nodiscard]] SavedDraw<Header> unpackDraw(std::span<const std::byte> bytes, const char *what) {
  static_assert(std::is_trivially_copyable_v<Header>);
  if (bytes.size() < sizeof(Header) || (bytes.size() - sizeof(Header)) % sizeof(LevelWord) != 0) {
    lucent::error("guestmem", "a saved {} of {} bytes is not one", what, bytes.size());
    std::abort();
  }
  SavedDraw<Header> saved;
  std::memcpy(&saved.header, bytes.data(), sizeof(Header));
  saved.reads.resize((bytes.size() - sizeof(Header)) / sizeof(LevelWord));
  std::memcpy(
      saved.reads.data(), bytes.data() + sizeof(Header), saved.reads.size() * sizeof(LevelWord));
  return saved;
}

} // namespace spyro::guest_memory
