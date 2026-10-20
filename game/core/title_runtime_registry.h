#pragma once

#include "core/runtime.h"
#include "spyro1/core/runtime/runtime.h"
#include "spyro2/core/runtime.h"
#include "spyro3/core/runtime.h"
#include "title_catalog.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace spyro {

enum class Title : std::uint8_t { Spyro1, Spyro2, Spyro3 };

// Spyro 1/2/3 as the multi-title host's catalog: entry i of the generated identities is title i.
class Catalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::filesystem::path provisioningRoot() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  Runtime &runtime(std::size_t index) const override;
  Title title(std::size_t index) const;

private:
  // The host asks for a mutable runtime from a const catalog; each runtime is its title's one.
  mutable spyro1::Runtime spyro1_;
  mutable spyro2::Runtime spyro2_;
  mutable spyro3::Runtime spyro3_;
};

// The product's catalog, run by the framework main (psxport_add_product).
std::unique_ptr<psx::host::TitleCatalog> createCatalog();

} // namespace spyro
