#pragma once

#include "core/field_owner.h"

#include <cstdint>

class Core;

namespace spyro1 {

// Spyro 1's per-field observer: the boot window a Start edge is read in, and the `skipmap` log of
// the screens a press could end.
class SkipMap final : public spyro::FieldObserver {
public:
  void bootSequenceBegin();
  void bootSequenceEnd();
  void onField(Core &core, bool startEdge) override;

private:
  bool bootSequenceActive_ = false;
  std::uint32_t fields_ = 0;
  std::uint32_t bootFields_ = 0;
  std::uint32_t stageFields_ = 0;
  std::uint32_t startEdges_ = 0;
  std::uint32_t previousGamestate_ = ~0u;
  std::uint32_t previousTitleMode_ = ~0u;
  std::uint32_t previousTitleStage_ = ~0u;
  std::uint32_t previousTitleSubState_ = ~0u;
  std::uint32_t previousLoadStage_ = ~0u;
  bool previousBootActive_ = false;
};

} // namespace spyro1
