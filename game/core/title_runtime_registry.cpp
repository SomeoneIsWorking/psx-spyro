#include "core/title_runtime_registry.h"

#include "spyro_title_catalog.generated.h"

#include <cstdlib>

namespace spyro {

std::string_view Catalog::productName() const {
  return "Spyro";
}

std::filesystem::path Catalog::provisioningRoot() const {
  return "scratch/assets";
}

std::span<const psx::host::TitleIdentity> Catalog::titles() const {
  return generated::kExecutableCatalog;
}

Title Catalog::title(std::size_t index) const {
  return generated::kCatalogTitles.at(index);
}

Runtime &Catalog::runtime(std::size_t index) const {
  switch (title(index)) {
  case Title::Spyro1:
    return spyro1_;
  case Title::Spyro2:
    return spyro2_;
  case Title::Spyro3:
    return spyro3_;
  }
  std::abort();
}

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<Catalog>();
}

} // namespace spyro
