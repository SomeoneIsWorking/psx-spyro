#pragma once

#include "spyro_runtime.h"
#include "title_catalog.h"

#include <filesystem>
#include <memory>
#include <span>

namespace spyro {

SpyroRuntime &runtimeFor(SpyroTitle title);

// Spyro 1/2/3 as the multi-title host's catalog: entry i of the generated identities is title i.
class SpyroCatalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::filesystem::path provisioningRoot() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  SpyroRuntime &runtime(std::size_t index) const override;
  SpyroTitle title(std::size_t index) const;
};

// The product's catalog, run by the framework main (psxport_add_product).
std::unique_ptr<psx::host::TitleCatalog> createCatalog();

} // namespace spyro
