#pragma once

#include <chrono>

#include <cstdint>

class Core;
class Game;

namespace spyro {

// One display field's request. `present` crosses the presentation fence, `pace` waits for the host
// deadline.
struct FieldRequest {
  const char *site;
  bool present;
  bool pace;
};

// Title facts the shared field owner cannot know; a zero rootHandlerSlot means the title has no
// guest vblank root.
struct FieldOwnerFacts {
  const char *titleName = "";
  // Advanced once per delivered field when the guest has no root that does it; the title's own
  // copy, so a title that reads it sees delivered fields.
  std::uint32_t fieldCounter = 0;
  std::uint32_t rootHandlerSlot = 0;
  // The word whose nonzero value makes the frame draw wait one field before its two-field loop; 0
  // when none.
  std::uint32_t drawWaitFlag = 0;
};

// The shared owner runs every title-neutral effect of a field, then asks the title to observe it.
class FieldObserver {
public:
  virtual ~FieldObserver() = default;
  virtual void onField(Core &core, bool startEdge) = 0;
};

// Counts fields delivered during one logic iteration.
class FieldCadence {
public:
  void beginLogicFrame() {
    fields_ = 0;
  }

  void delivered() {
    ++fields_;
  }

  std::uint32_t fields() const {
    return fields_;
  }

private:
  std::uint32_t fields_ = 0;
};

// The one definition of "a display field happened"; everything title-neutral a field does.
class FieldOwner {
public:
  // A title with nothing to observe per field passes no observer.
  FieldOwner(Game &game, FieldOwnerFacts facts, FieldObserver *observer = nullptr);

  void publish();

  bool deliver(FieldRequest request);
  // Crosses the presentation fence with no field of its own, paced over every field delivered since
  // the last paced one: the frame a guest drew after its own field waits.
  void presentFrame();

  void beginLogicFrame() {
    cadence_.beginLogicFrame();
  }

  std::uint32_t fieldsThisLogicFrame() const {
    return cadence_.fields();
  }

  std::int32_t counter() const;
  void armHostClock();

  // Whether the Start/Cross edge that ends a presentation-only hold is down now.
  bool presentationSkipPressed() const;

  std::uint64_t fields() const {
    return fields_;
  }

  std::uint64_t presents() const {
    return presents_;
  }

  // Fields delivered since the last paced one: the logic frame's so far.
  int fieldsSincePace() const {
    return static_cast<int>(fieldsSincePace_);
  }

  // The fields the frame being drawn is presented over: its draw waits until two have passed since
  // the last frame, after a wait of its own while the draw-wait flag is set.
  int drawnFrameFields() const;

  // The host-turn entry point: one more physical field, never a second presentation fence.
  static void hostTurnThunk(Core *core);

private:
  // Whether a guest root owns the field counter, including while its IRQ is deferred.
  bool dispatchCallbacks();
  void reportField(const FieldRequest &request);
  // The fields a paced present waits out: every one delivered since the previous paced present.
  int takePacedFields(bool pace);

  Game &game_;
  FieldOwnerFacts facts_;
  FieldObserver *observer_;
  FieldCadence cadence_{};
  bool inField_ = false;
  bool hostClockArmed_ = false;
  std::uint64_t refused_ = 0;
  std::uint64_t fields_ = 0;
  std::uint64_t paces_ = 0;
  std::uint64_t fieldsSincePace_ = 0;
  std::uint64_t presents_ = 0;

  // Per owner, so two titles in one process do not share an elapsed time.
  std::chrono::steady_clock::time_point epoch_ = std::chrono::steady_clock::now();
};

} // namespace spyro
