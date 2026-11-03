// The title screen's logo gate.
#pragma once

#include "core/title_logo_facts.h"

class Core;

namespace spyro {

// Whether the title screen waits with its finished logo up right now.
bool titleLogoGateOpen(Core &core, const TitleLogoFacts &facts);

} // namespace spyro
