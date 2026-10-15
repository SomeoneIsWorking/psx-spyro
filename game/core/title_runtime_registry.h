#pragma once

#include "spyro1_runtime.h"
#include "spyro2_runtime.h"
#include "spyro3_runtime.h"
#include "spyro_runtime.h"
#include "title_catalog.h"

#include <filesystem>
#include <memory>
#include <span>

namespace spyro {

// Spyro 1/2/3 as the multi-title host's catalog: entry i of the generated identities is title i.
class SpyroCatalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::filesystem::path provisioningRoot() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  SpyroRuntime &runtime(std::size_t index) const override;
  SpyroTitle title(std::size_t index) const;

private:
  // The host asks for a mutable runtime from a const catalog; each runtime is its title's one.
  mutable spyro1::Spyro1Runtime spyro1_;
  mutable spyro2::Spyro2Runtime spyro2_;
  mutable spyro3::Spyro3Runtime spyro3_;
};

// The product's catalog, run by the framework main (psxport_add_product).
std::unique_ptr<psx::host::TitleCatalog> createCatalog();

} // namespace spyro
