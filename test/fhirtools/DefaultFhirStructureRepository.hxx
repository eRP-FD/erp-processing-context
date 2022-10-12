// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef DEFAULTFHIRSTRUCTUREREPOSITORY_H
#define DEFAULTFHIRSTRUCTUREREPOSITORY_H

#include <filesystem>
#include <list>
#include <memory>
#include <string>

namespace fhirtools {
class FhirStructureRepository;
}

class DefaultFhirStructureRepository
{
public:
    static std::list<std::filesystem::path> defaultProfileFiles();

    static const fhirtools::FhirStructureRepository& get();
    static const fhirtools::FhirStructureRepository& getWithTest();

    static std::unique_ptr<const fhirtools::FhirStructureRepository> create(const std::list<std::filesystem::path>&);

private:

};

#endif// DEFAULTFHIRSTRUCTUREREPOSITORY_H
