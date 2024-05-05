// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
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
class FhirStructureRepositoryBackend;
}

class DefaultFhirStructureRepository
{
public:
    static std::list<std::filesystem::path> defaultProfileFiles();

    static const fhirtools::FhirStructureRepositoryBackend& get();
    static const fhirtools::FhirStructureRepositoryBackend& getWithTest();

    static std::unique_ptr<const fhirtools::FhirStructureRepositoryBackend>
    create(const std::list<std::filesystem::path>&);

private:
};

#endif// DEFAULTFHIRSTRUCTUREREPOSITORY_H
