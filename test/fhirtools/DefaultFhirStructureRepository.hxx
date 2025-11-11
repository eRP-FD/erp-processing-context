// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#ifndef DEFAULTFHIRSTRUCTUREREPOSITORY_H
#define DEFAULTFHIRSTRUCTUREREPOSITORY_H

#include <filesystem>
#include <list>
#include <memory>
#include <string>

namespace fhirtools
{
class FhirStructureRepositoryView;
class FhirStructureRepositoryBackend;
class FhirResourceGroupResolver;
}

class DefaultFhirStructureRepository
{
public:
    static std::list<std::filesystem::path> defaultProfileFiles();

    static const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& get();

    static const fhirtools::FhirStructureRepositoryBackend& getBackendWithTest();
    static const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& getWithTest();

    static std::unique_ptr<const fhirtools::FhirStructureRepositoryBackend>
    create(const std::list<std::filesystem::path>&, const fhirtools::FhirResourceGroupResolver& resolver);

private:
};

#endif// DEFAULTFHIRSTRUCTUREREPOSITORY_H
