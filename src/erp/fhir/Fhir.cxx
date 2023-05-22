/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Fhir.hxx"
#include "erp/util/Configuration.hxx"

#include <algorithm>

const Fhir& Fhir::instance()
{
    static Fhir theInstance;
    return theInstance;
}

Fhir::Fhir()
    : mConverter()
    , mStructureRepository()
{
    if (Configuration::instance().getOptionalStringValue(ConfigurationKey::ERP_FHIR_VERSION_OLD))
    {
        loadVersion(ConfigurationKey::ERP_FHIR_VERSION_OLD, ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS_OLD);
    }
    loadVersion(ConfigurationKey::ERP_FHIR_VERSION, ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS);
}

void Fhir::loadVersion(ConfigurationKey versionKey, ConfigurationKey structureKey)
{
    auto versionAsString = Configuration::instance().getStringValue(versionKey);
    auto version = model::ResourceVersion::str_vBundled(versionAsString);
    auto filesAsString = Configuration::instance().getArray(structureKey);
    std::list<std::filesystem::path> files;
    std::transform(filesAsString.begin(), filesAsString.end(), std::back_inserter(files), [](const auto& str) {
        return std::filesystem::path{str};
    });
    TLOG(INFO) << "Loading FHIR structure repository for " << magic_enum::enum_name(version);
    mStructureRepository[version].load(files);
}
