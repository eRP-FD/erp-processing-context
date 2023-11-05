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

    if (model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01 == version)
    {
        const auto patchVersion = model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01_patch;
        auto patchReplace =
            Configuration::instance().getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_PATCH_REPLACE);
        auto patchReplaceWith =
            Configuration::instance().getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_PATCH_REPLACE_WITH);
        auto patchValidFrom =
            Configuration::instance().getOptionalStringValue(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM);
        Expect3(patchReplace && patchReplaceWith && patchValidFrom,
                "please check fhir package configuration or remove hard-coded kbv 1.1.2 patch if obsolete.",
                std::logic_error);
        TLOG(INFO) << "Loading PATCHED FHIR structure repository for " << magic_enum::enum_name(patchVersion);
        bool replaced = false;
        for (auto& file : files)
        {
            if (std::filesystem::equivalent(file, std::filesystem::path{*patchReplace}))
            {
                LOG(INFO) << "FHIR profile patch replacing " << file.string() << " with " << *patchReplaceWith;
                file = std::filesystem::path{*patchReplaceWith};
                replaced = true;
            }
        }
        Expect(replaced, "patch not applied, check configuration! FHIR_PROFILE_PATCH_REPLACE=" + *patchReplace +
                             " FHIR_PROFILE_PATCH_REPLACE_WITH=" + *patchReplaceWith);
        mStructureRepository[patchVersion].load(files);
    }
}
