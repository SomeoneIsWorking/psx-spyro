#pragma once

#include <chrono>

#include <cstdint>
#include <string_view>

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
  // The guest IRQ stack the vblank callbacks run on.
  std::uint32_t handlerStackTop = 0;
  std::uint32_t handlerStackBytes = 0;
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

// The one definition of "a display field happened": sample the pad, advance the
// display clock, run the guest vblank callbacks, advance the field counter, let the title observe,
// snapshot, cross the presentation fence when visible, service audio, release the host-turn token.
class FieldOwner {
public:
  // A title with nothing to observe per field passes no observer.
  FieldOwner(Game &game, FieldOwnerFacts facts, FieldObserver *observer = nullptr);

  // Publication reaches the shared context, so it is an explicit call rather than constructor
  // work that makes every owner's member order a hidden precondition.
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
  std::string_view activeDeliverySite() const;
  void armHostClock();
  void observeVblankCallback(std::uint32_t function);

  // Whether the Start/Cross edge that ends a presentation-only hold is down now.
  bool presentationSkipPressed() const;

  std::uint64_t fields() const {
    return fields_;
  }

  std::uint64_t presents() const {
    return presents_;
  }

  std::uint64_t paces() const {
    return paces_;
  }

  const FieldOwnerFacts &facts() const {
    return facts_;
  }

  // The host-turn entry point: one more physical field, never a second presentation fence.
  static void hostTurnThunk(Core *core);

private:
  // True while a guest root owns its counter, including an IRQ deferred by masking or critical
  // state. Returns whether the guest root ticked the counter itself.
  bool dispatchCallbacks();
  void armHandlerStack(Core &core);
  void reportField(const FieldRequest &request);
  // The fields a paced present waits out: every one delivered since the previous paced present.
  int takePacedFields(bool pace);

  Game &game_;
  FieldOwnerFacts facts_;
  FieldObserver *observer_;
  FieldCadence cadence_{};
  bool inField_ = false;
  const char *activeDeliverySite_ = nullptr;
  bool handlerStackArmed_ = false;
  bool hostClockArmed_ = false;
  std::uint64_t refused_ = 0;
  std::uint64_t fields_ = 0;
  std::uint64_t paces_ = 0;
  std::uint64_t fieldsSincePace_ = 0;
  std::uint64_t presents_ = 0;

  std::uint32_t callbackFallback_ = 0;
  std::uint16_t previousButtons_ = 0xFFFFu;
  // Wall-clock epoch of this owner; a `pace` report needs milliseconds since the run began, and
  // two titles in one process would otherwise print each other's elapsed time.
  std::chrono::steady_clock::time_point epoch_ = std::chrono::steady_clock::now();
};

// The process-lifetime field owner of the selected title, published by its frame driver. A host
// turn can arrive before the driver exists, so this refuses by name instead of guessing.
FieldOwner &fieldOwner(Core &core);
const FieldOwner &fieldOwner(const Core &core);

} // namespace spyro
