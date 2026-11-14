#include "spyro3/core/title_state.h"

namespace spyro3 {

TitleState &titleState(Core &core) {
  return static_cast<TitleState &>(*spyro::session(core).titleState);
}

} // namespace spyro3
