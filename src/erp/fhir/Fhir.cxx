/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Fhir.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Configuration.hxx"
#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"

#include <fhirtools/repository/FhirResourceViewVerifier.hxx>
#include <algorithm>


const Fhir& Fhir::instance()
{
    static Fhir theInstance;
    return theInstance;
}

Fhir::Fhir()
    : mConverter()
    , mBackend()
{
    load(ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS);
    validateViews();
}

void Fhir::load(ConfigurationKey structureKey)
{
    const auto& config = Configuration::instance();
    auto filesAsString = config.getArray(structureKey);
    std::list<std::filesystem::path> files;
    std::transform(filesAsString.begin(), filesAsString.end(), std::back_inserter(files), [](const auto& str) {
        return std::filesystem::path{str};
    });
    TLOG(INFO) << "Loading FHIR structure repository.";
    auto resolver = config.fhirResourceGroupConfiguration();
    for (const auto& [url, version] : config.synthesizeCodesystem())
    {
        mBackend.synthesizeCodeSystem(url, version, resolver);
    }
    for (const auto& [url, version] : config.synthesizeValuesets())
    {
        mBackend.synthesizeValueSet(url, version, resolver);
    }
    mBackend.load(files, resolver);
}

fhirtools::FhirResourceViewConfiguration::ViewList
Fhir::structureRepository(const model::Timestamp& referenceTimestamp) const
{
    const auto& config = Configuration::instance();

    auto viewConfig = config.fhirResourceViewConfiguration();
    fhirtools::FhirResourceViewConfiguration::ViewList viewList;
    const auto& referenceDate = fhirtools::Date{referenceTimestamp.toGermanDate()};
    viewList = viewConfig.getViewInfo(referenceDate);
    Expect(! viewList.empty(), "invalid reference date: " + referenceDate.toString(false));
    return viewList;
}


void Fhir::validateViews() const
{
    const auto& config = Configuration::instance();
    auto viewConfig = config.fhirResourceViewConfiguration();
    for (const auto& viewConf : viewConfig.allViews())
    {
        auto view = viewConf->view(&mBackend);
        fhirtools::FhirResourceViewVerifier verifier{mBackend, view.get().get()};
        verifier.verify();
        for (const auto profileType : magic_enum::enum_values<model::ProfileType>())
        {
            if (profileType != model::ProfileType::fhir)
            {
                const auto key = profileWithVersion(profileType, *view);
                Expect3(key.has_value(),
                        "could not resolve " + std::string{magic_enum::enum_name(profileType)} +
                            " in view: " + std::string{view->id()},
                        std::logic_error);
                TVLOG(2) << view->id() << " found profile: " << *key;
            }
        }
    }
}
