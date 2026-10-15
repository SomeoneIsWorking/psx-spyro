#include "title_runtime_registry.h"

#include "spyro_title_catalog.generated.h"

#include <cstdlib>

namespace spyro {

std::string_view SpyroCatalog::productName() const {
  return "Spyro";
}

std::filesystem::path SpyroCatalog::provisioningRoot() const {
  return "scratch/assets";
}

std::span<const psx::host::TitleIdentity> SpyroCatalog::titles() const {
  return generated::kExecutableCatalog;
}

SpyroTitle SpyroCatalog::title(std::size_t index) const {
  return generated::kCatalogTitles.at(index);
}

SpyroRuntime &SpyroCatalog::runtime(std::size_t index) const {
  switch (title(index)) {
  case SpyroTitle::Spyro1:
    return spyro1_;
  case SpyroTitle::Spyro2:
    return spyro2_;
  case SpyroTitle::Spyro3:
    return spyro3_;
  }
  std::abort();
}

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<SpyroCatalog>();
}

} // namespace spyro
