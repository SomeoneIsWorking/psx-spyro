#include "core/field_owner.h"

#include "core.h"
#include "core/session.h"
#include "core/vblank_irq.h"
#include "frame_pacer.h"
#include "game.h"
#include "guest_call.h"
#include "hle.h"
#include "host_turn.h"
#include "snapshot.h"

#include <chrono>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro {
namespace {

constexpr std::uint16_t kPadStart = 0x0008u;
constexpr std::uint16_t kPadCross = 0x4000u;
constexpr std::uint32_t kStackPoison = 0xCDCDCDCDu;

} // namespace

FieldOwner::FieldOwner(Game &game, FieldOwnerFacts facts, FieldObserver *observer)
    : game_(game), facts_(facts), observer_(observer) {}

void FieldOwner::publish() {
  spyro::session(game_.core).fieldOwner = this;
}

std::int32_t FieldOwner::counter() const {
  return static_cast<std::int32_t>(game_.core.mem_r32(facts_.fieldCounter));
}

std::string_view FieldOwner::activeDeliverySite() const {
  return inField_ && activeDeliverySite_ != nullptr ? std::string_view(activeDeliverySite_)
                                                    : std::string_view{};
}

void FieldOwner::armHostClock() {
  if (hostClockArmed_) {
    lucent::error("fields", "{} host field clock armed twice", facts_.titleName);
    std::abort();
  }
  hostClockArmed_ = true;
  psx::cpu::registerHostTurn(
      game_.core, &FieldOwner::hostTurnThunk, game_.framePacer.fieldRateMilliHz(game_.core));
  lucent::info("fields", "{} host field clock armed at field {}", facts_.titleName, counter());
}

void FieldOwner::observeVblankCallback(std::uint32_t function) {
  if (function == callbackFallback_) {
    return;
  }
  callbackFallback_ = function;
  lucent::info("fields",
               "{} VSyncCallback(0x{:08X}) registered for host-owned field delivery",
               facts_.titleName,
               function);
}

void FieldOwner::armHandlerStack(Core &core) {
  const std::uint32_t floor = facts_.handlerStackTop - facts_.handlerStackBytes;
  for (std::uint32_t address = floor; address < facts_.handlerStackTop; address += 4) {
    core.mem_w32(address, kStackPoison);
  }
  lucent::info("fields",
               "{} guest vblank callbacks use IRQ stack [0x{:08X},0x{:08X})",
               facts_.titleName,
               floor,
               facts_.handlerStackTop);
}

bool FieldOwner::dispatchCallbacks() {
  Core &core = game_.core;
  const std::uint32_t root =
      facts_.rootHandlerSlot != 0 ? core.mem_r32(facts_.rootHandlerSlot) : 0u;
  const std::uint32_t target = root != 0 ? root : callbackFallback_;
  if (target == 0) {
    // The guest's own chain takes this field's edge now; a following field would latch over it.
    if (spyro::hasPendingEnabledVblank(core.irqStatLatch(), game_.hle.i_mask) &&
        game_.hle.canDispatchInterrupt(core)) {
      game_.hle.irqPoll(&core);
    }
    return false;
  }
  const bool rootOwnsIrq = root != 0 && game_.hle.exception_exit_buf != 0;
  if (rootOwnsIrq && (!spyro::hasPendingEnabledVblank(core.irqStatLatch(), game_.hle.i_mask) ||
                      !game_.hle.canDispatchInterrupt(core))) {
    return true;
  }
  if (!handlerStackArmed_) {
    handlerStackArmed_ = true;
    armHandlerStack(core);
  }

  const std::int32_t before = counter();
  R3000 saved = static_cast<R3000 &>(core);
  core.r[29] = facts_.handlerStackTop;
  if (rootOwnsIrq) {
    core.pending_work |= Core::PW_IRQ;
    game_.hle.irqPoll(&core);
  } else {
    psx::cpu::dispatchGuestToReturn0(
        core, target, psx::cpu::ExecutionBudget::currentTurn(core), "field-callback");
  }
  static_cast<R3000 &>(core) = saved;

  if (core.mem_r32(facts_.handlerStackTop - facts_.handlerStackBytes) != kStackPoison) {
    lucent::error("fields",
                  "{} guest vblank callback overflowed its {}-byte IRQ stack at 0x{:08X}",
                  facts_.titleName,
                  facts_.handlerStackBytes,
                  facts_.handlerStackTop - facts_.handlerStackBytes);
    std::abort();
  }

  const std::int32_t after = counter();
  if (root != 0 && after != before + 1) {
    lucent::error("fields",
                  "{} guest vblank root advanced counter {} -> {}; host scheduler requires exactly "
                  "one field",
                  facts_.titleName,
                  before,
                  after);
    std::abort();
  }
  return root != 0;
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
  activeDeliverySite_ = request.site;

  game_.pad.serviceFrame();
  const bool startDown = (game_.pad.buttons & kPadStart) == 0;
  const bool startEdge = startDown && (previousButtons_ & kPadStart) != 0;
  previousButtons_ = game_.pad.buttons;
  if (!game_.timing.advanceDisplayFields(1, 1, core.game->framePacer.fieldRateMilliHz(core))) {
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
  snapshot_tick(&core);

  if (request.present) {
    game_.presentation.commit(&core, takePacedFields(request.pace));
    ++presents_;
  } else if (request.pace) {
    game_.framePacer.waitPresentedFields(core, takePacedFields(request.pace), 1);
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
  activeDeliverySite_ = nullptr;
  inField_ = false;
  return true;
}

void FieldOwner::presentFrame() {
  game_.presentation.commit(&game_.core, takePacedFields(true));
  ++presents_;
  ++paces_;
}

bool FieldOwner::presentationSkipPressed() const {
  return game_.pad.pressedButton(kPadStart | kPadCross);
}

void FieldOwner::hostTurnThunk(Core *core) {
  fieldOwner(*core).deliver({.site = "hostturn", .present = false, .pace = false});
}

FieldOwner &fieldOwner(Core &core) {
  FieldOwner *owner = spyro::session(core).fieldOwner;
  if (owner == nullptr) {
    lucent::error("fields", "no title field owner is published for this Core");
    std::abort();
  }
  return *owner;
}

const FieldOwner &fieldOwner(const Core &core) {
  const FieldOwner *owner = spyro::session(core).fieldOwner;
  if (owner == nullptr) {
    lucent::error("fields", "no title field owner is published for this Core");
    std::abort();
  }
  return *owner;
}

} // namespace spyro
