// drawn_records.h — the mobys a draw builds itself each draw on the g_HudMobys arena (menu items,
// dialogue and title letters), whose spins and slides come from a count the draw reads whole
// (the menus' D_800758B8, the balloonist's D_800777E8 +0x04). Each build clears its record with
// Memset(g_HudMobys, 0, sizeof(Moby)). Inside a text, digit or cursor builder (0x800181AC,
// 0x80017FE4, 0x80018534) a record is the builder's character: the jal site that called it, that
// call's turn at the site in the draw (the caller's loop: a dialogue line, an egg slot) and the
// builder's own index, the non-space characters of pText before it. A draw's own build is its site
// and its turn there (Flight5's trophy, the credits' entry loop). A replay's record is moved from
// the last real draw's with the same name and mesh.
#pragma once

#include "render/field/moby_record.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class Core;

namespace spyro1::actors {

class DrawnRecords {
public:
  // A GamestateDraw starts; its builds are named afresh, and a real one's replace the saved ones.
  enum class Draw : std::uint8_t { Real, Replay };
  void beginDraw(Draw draw);

  // A builder at `entry` was called from `caller`; the builds it makes until it returns are named
  // by that call.
  struct Call {
    std::uint32_t caller = 0;
    std::uint32_t turn = 0;
    std::uint32_t built = 0;
  };
  struct Builder {
    Call call;
  };
  [[nodiscard]] Builder enterBuilder(std::uint32_t caller);
  void leaveBuilder(Builder outer);

  // A fill of `bytes` at `address` returning to `site`: a build when it clears one moby at
  // g_HudMobys.
  struct Fill {
    std::uint32_t address = 0;
    std::uint32_t bytes = 0;
    std::uint32_t site = 0;
  };
  void filled(Core &core, const Fill &fill);

  // The draw queued `record`. Outside a replay it is saved as the real draw's; inside one it is
  // written with its position and angles the replay's `t` of the way from the last real draw's
  // record of the same name and mesh, and the angles' sixteenths are returned.
  std::optional<std::uint32_t> queued(Core &core, std::uint32_t record);

private:
  struct Name {
    std::uint32_t site = 0;
    std::uint32_t caller = 0;
    std::uint32_t turn = 0;
    std::uint32_t index = 0;
    bool operator==(const Name &) const = default;
  };
  [[nodiscard]] std::optional<Name> nameOf(std::uint32_t record) const;

  // The outermost builder call running, and each caller's builder calls this draw.
  Call call_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> calls_;
  // This draw's builds, by record address, newest last.
  std::vector<std::pair<std::uint32_t, Name>> built_;
  // The latest real draw's; a frame's replays run before its real draw.
  std::vector<std::pair<Name, spyro::moby_record::Bytes>> drawn_;
};

// The text and cursor builders, wrapped so their builds are named by their caller.
void registerBuildSites(Core &core);

} // namespace spyro1::actors
