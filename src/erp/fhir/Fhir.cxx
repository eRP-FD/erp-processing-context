/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "Fhir.hxx"

#include "erp/fhir/FhirStructureRepository.hxx"
#include "erp/util/Configuration.hxx"

#include <algorithm>

const Fhir& Fhir::instance()
{
    static Fhir theInstance;
    return theInstance;
}

Fhir::Fhir() :
    mConverter(),
    mStructureRepository()
{
    auto filesAsString = Configuration::instance().getArray(ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS);
    std::list<std::filesystem::path> files;
    std::transform(filesAsString.begin(), filesAsString.end(), std::back_inserter(files),
                   [](const auto& str){ return std::filesystem::path{str};} );
    mStructureRepository.load(files);
}
