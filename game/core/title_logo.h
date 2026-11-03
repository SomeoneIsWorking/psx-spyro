// A title's own logo, rebuilt from its title screen's draws while that screen shows it.
#pragma once

#include "core/title_logo_facts.h"
#include "logo_image.h"

#include <optional>

class Core;

namespace spyro {

// The wordmark as the last committed frame drew it from the named pages; nullopt while the gate is
// shut or nothing was drawn from them.
std::optional<psx::host::LogoImage> extractPanelLogo(Core &core, const TitleLogoFacts &facts);

// Whether the title screen waits with its finished logo up right now.
bool titleLogoGateOpen(Core &core, const TitleLogoFacts &facts);

} // namespace spyro
