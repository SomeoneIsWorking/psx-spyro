#pragma once

#include "core.h"
#include "execution_control.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "r3000.h"

#include <cstdint>

namespace spyro {

// A retail body returns with the `$ra` it entered with; declare this first in an override.
class PreservedReturnAddress {
public:
  explicit PreservedReturnAddress(Core &core) : core_(core), entry_(core.r[psx::cpu::kRa]) {}
  PreservedReturnAddress(const PreservedReturnAddress &) = delete;
  PreservedReturnAddress &operator=(const PreservedReturnAddress &) = delete;
  ~PreservedReturnAddress() {
    core_.r[psx::cpu::kRa] = entry_;
  }

private:
  Core &core_;
  std::uint32_t entry_;
};

// A callee reading its arguments from `$sp` sees the retail body's lowered frame for the scope.
class GuestFrameScope {
public:
  GuestFrameScope(Core &core, std::uint32_t frame) : core_(core), entry_(core.r[psx::cpu::kSp]) {
    core_.r[psx::cpu::kSp] = frame;
  }
  GuestFrameScope(const GuestFrameScope &) = delete;
  GuestFrameScope &operator=(const GuestFrameScope &) = delete;
  ~GuestFrameScope() {
    core_.r[psx::cpu::kSp] = entry_;
  }

private:
  Core &core_;
  std::uint32_t entry_;
};

} // namespace spyro
