#include "core/field_owner.h"

#include "core.h"
#include "core/session.h"
#include "field_rate.h"
#include "frame_pacer.h"
#include "game.h"
#include "guest_call.h"
#include "hle.h"
#include "host_input.h"
#include "host_turn.h"
#include "irq_edge.h"

#include <chrono>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro {

FieldOwner::FieldOwner(Game &game, FieldOwnerFacts facts, FieldObserver *observer)
    : game_(game), facts_(facts), observer_(observer) {}

void FieldOwner::publish() {
  spyro::session(game_.core).fieldOwner = this;
}

std::int32_t FieldOwner::counter() const {
  return static_cast<std::int32_t>(game_.core.mem_r32(facts_.fieldCounter));
}

void FieldOwner::armHostClock() {
  if (hostClockArmed_) {
    lucent::error("fields", "{} host field clock armed twice", facts_.titleName);
    std::abort();
  }
  hostClockArmed_ = true;
  Core &core = game_.core;
  core.hostTurn.arm(core, &FieldOwner::hostTurnThunk, psx::frame::fieldRateMilliHz(game_.timing.displayFieldRate()));
  lucent::info("fields", "{} host field clock armed at field {}", facts_.titleName, counter());
}

bool FieldOwner::dispatchCallbacks() {
  Core &core = game_.core;
  const std::uint32_t root = facts_.rootHandlerSlot != 0 ? core.mem_r32(facts_.rootHandlerSlot) : 0u;
  const bool vblankDeliverable = core.irqPendingEnabled(IRQ_BIT_VBLANK) && game_.hle.canDispatchInterrupt(core);
  if (root == 0) {
    // The guest's own chain takes this field's edge now; a following field would latch over it.
    if (vblankDeliverable) {
      game_.hle.irqPoll(&core);
    }
    return false;
  }
  const std::int32_t before = counter();
  if (game_.hle.exception_exit_buf != 0) {
    if (!vblankDeliverable) {
      return true;
    }
    core.pending_work |= Core::PW_IRQ;
    game_.hle.irqPoll(&core);
  } else {
    const Hle::InterruptScope scope(game_.hle, core);
    psx::cpu::callGuestNow(core, "field-callback", root);
  }
  const std::int32_t after = counter();
  if (after != before + 1) {
    lucent::error("fields",
                  "{} guest vblank root advanced counter {} -> {}; host scheduler requires exactly one field",
                  facts_.titleName,
                  before,
                  after);
    std::abort();
  }
  return true;
}

void FieldOwner::reportField(const FieldRequest &request) {
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - epoch_).count();
  lucent::debug("pace",
                "t={:.1f}ms vbl={} pace={} present={} refused={} | site={} counter={}",
                elapsedMs,
                fields_,
                paces_,
                presents_,
                refused_,
                request.site,
                counter());
}

int FieldOwner::takePacedFields(bool pace) {
  if (!pace) {
    return 0;
  }
  const auto fields = static_cast<int>(fieldsSincePace_);
  fieldsSincePace_ = 0;
  return fields;
}

bool FieldOwner::deliver(FieldRequest request) {
  Core &core = game_.core;
  if (inField_) {
    ++refused_;
    lucent::debug("pace",
                  "field refused at {}: one is already in flight (refused={})",
                  request.site,
                  refused_);
    return false;
  }
  inField_ = true;

  game_.pad.serviceFrame();
  const bool startEdge = game_.pad.pressedButton(psx::input::kButtonStart);
  if (!game_.timing.advanceDisplayFields({1, 1}, psx::frame::fieldRateMilliHz(game_.timing.displayFieldRate()))) {
    lucent::error(
        "fields", "{} display clock refused one field at {}", facts_.titleName, request.site);
    std::abort();
  }
  const bool guestRootOwnsCounter = dispatchCallbacks();
  if (!guestRootOwnsCounter && facts_.fieldCounter != 0) {
    core.mem_w32(facts_.fieldCounter, core.mem_r32(facts_.fieldCounter) + 1u);
  }
  cadence_.delivered();
  ++fieldsSincePace_;
  if (observer_ != nullptr) {
    observer_->onField(core, startEdge);
  }
  if (request.present) {
    game_.presentation.commit(&core, takePacedFields(request.pace));
    ++presents_;
  } else if (request.pace) {
    game_.framePacer.waitPresentedFields(core, {takePacedFields(request.pace), 1});
  }
  game_.spu_audio.frame();
  if (request.pace) {
    ++paces_;
  }
  // The BIOS VBlank and root-counter-3 event classes a field raises.
  for (const std::uint32_t eventClass : {0xF0000009u, 0xF2000003u}) {
    game_.hle.deliverEvent(eventClass, 0xFFFFFFFFu);
  }

  ++fields_;
  reportField(request);
  inField_ = false;
  return true;
}

void FieldOwner::presentFrame() {
  game_.presentation.commit(&game_.core, takePacedFields(true));
  ++presents_;
  ++paces_;
}

bool FieldOwner::presentationSkipPressed() const {
  return game_.pad.pressedButton(psx::input::kButtonStart | psx::input::kButtonCross);
}

void FieldOwner::hostTurnThunk(Core *core) {
  FieldOwner *owner = spyro::session(*core).fieldOwner;
  if (owner == nullptr) {
    lucent::error("fields", "no title field owner is published for this Core");
    std::abort();
  }
  owner->deliver({.site = "hostturn", .present = false, .pace = false});
}

} // namespace spyro
