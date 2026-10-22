// field_redraw.h — everything the field's draw dispatcher draws but the terrain, drawn once more by
// the guest's own routine for the in-between before each real field, and owned at the flatten.
//
// The dispatcher's override reads what an in-between moves (the camera, each moby record's
// position, angles and keyframes, Spyro's, and the effects pass's records), runs the dispatcher as
// a psx::present::GuestRedraw with those inputs at each in-between's t from the last field's, keeps
// what the bins and the overlay pair then hold (the packets logic linked before the dispatcher and
// the ones the redraw linked), and then makes the real call. It redraws once per in-between of a
// frame as long as the last one. The terrain has its own producer, so the redraw clears its flag.
// The real call may run on past the host turn, so the field is owned where its bins are whole: the
// flatten is the producer, keys every packet linked before the dispatcher's last drawer returned
// that the terrain did not, and saves the redraw.
//
// A screen draw (a game state's draw that calls the dispatcher and links its own 2D after it, as
// the title screen and its menus do) is redrawn whole in its place: the dispatcher inside it draws
// without the terrain in the redraw and as it is in the real call, and the flatten keys every
// packet the frame linked that the terrain did not.
#pragma once

#include "core.h"
#include "render/field/effect_pools.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/terrain_rebuild.h"
#include "state_producer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace spyro::field_redraw {

// A game state's draw redrawn whole; its overlay draw links 2D after the dispatcher returns.
struct ScreenDraw {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // The content identity of the loaded overlay holding `entry`; 0 when the executable holds it.
  std::uint64_t overlay = 0;
};

struct Facts {
  std::uint32_t entry = 0; // the dispatcher; 0 when the image has none
  const char *overrideName = "";
  std::uint32_t terrainFlag = 0;        // the $a0 bit that draws the terrain
  std::uint32_t drawCameraPosition = 0; // the draw's copy of the camera position, x, y, z words
  std::uint32_t spyroPosition = 0;      // x, y, z words
  std::uint32_t spyroAngles = 0;        // three angle bytes, then one that is not
  std::uint32_t spyroHeadAngles = 0;    // the same for the head
  std::uint32_t spyroKeyframes = 0;     // keyframe_blend::Pose keyframes, then the head's
  std::uint32_t spyroKeyProgress = 0;   // the body's progress byte, then the head's
  effect_pools::Facts effects;
  std::uint32_t lastDrawer = 0; // the dispatcher's last call, after which it links nothing
  const char *lastDrawerName = "";
  std::span<const ScreenDraw> screenDraws;
};

struct Image {
  const Facts &field;
  const guest_terrain::TerrainImage &terrain;
  std::uint32_t mobyArrayWord = 0;
};

// What one field's draw saw of the inputs an in-between moves.
struct Inputs {
  std::uint32_t serial = 0; // the flatten scope that saved this field, 0 until it has
  guest_terrain::CameraState camera;
  std::uint32_t mobyArray = 0;
  std::vector<moby_record::Bytes> mobys;
  struct Spyro {
    std::array<std::uint32_t, 3> position{};
    std::uint32_t angles = 0;
    std::uint32_t headAngles = 0;
    std::array<std::uint32_t, 2> keyframes{}; // body, head
    std::uint32_t keyProgress = 0;            // body byte, head byte
  } spyro;
  effect_pools::Records effects;
};

// The last field's inputs, the redraw waiting for its flatten, and the buffers every field reuses.
struct History {
  Inputs last;
  Inputs now;
  bool pending = false;
  bool redrawn = false;
  bool screen = false;    // the waiting field is a screen draw's, so the flatten keys all it linked
  bool redrawing = false; // a redraw is running, so the dispatcher inside it only draws
  // The primitive cursor once the dispatcher's last drawer returned; 0 until it has.
  std::uint32_t packetsEnd = 0;
  std::uint32_t lerpedFrom = 0;
  std::uint32_t fields = 0; // the cadence the redraws were made for
  // The bin the redraws' sky linked into, one past the mark they ran with, and the deepest bin the
  // last real field walked, which its sky took.
  std::uint32_t redrawSkyBin = 0;
  std::uint32_t lastSkyBin = 0;
  std::vector<std::byte> state;
};

// The dispatcher's override body: the redraw, then the real call; inside a screen draw, only the
// call.
void redrawField(Core &core, const Image &image);

// A screen draw's override body: the redraw of `entry`, then the real call.
void redrawScreen(Core &core, const Image &image, std::uint32_t entry);

// The last drawer's override body: the real call, then where the field's packets end.
void endField(Core &core, const Image &image);

// What the flatten claims for the field: every packet linked before the dispatcher's end, which the
// redraw reproduces, all linked so far after a screen draw, and none when no dispatcher ran to its
// end since the last flatten.
[[nodiscard]] depth_bins::Claim fieldClaim(Core &core,
                                           const guest_render_globals::Globals &globals);

// Inside the flatten's producer scope, after its bins are claimed: the waiting redraw saved as the
// field's state, its sky moved to the deeper of the two real fields' sky bins. Nothing when no
// dispatcher ran since the last flatten.
void saveField(Core &core, const guest_render_globals::Globals &globals);

class FieldRedrawProducer final : public psx::present::StateProducer {
public:
  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
};

} // namespace spyro::field_redraw
