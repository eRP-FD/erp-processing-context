/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirKbvKeyTablesView.hxx"
#include "erp/util/Configuration.hxx"

#include <utility>

namespace fhirtools
{

FhirKbvKeyTablesView::FhirKbvKeyTablesView(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                           const fhirtools::FhirResourceViewConfiguration& config,
                                           const fhirtools::Date& referenceDate)
    : DefaultFhirStructureRepositoryView(std::move(backend))
    , mConfig(config)
    , mReferenceDate(referenceDate)
{
}

const FhirValueSet* FhirKbvKeyTablesView::findValueSet(const std::string_view url,
                                                       std::optional<std::string> version) const
{
    if (! version)
    {
        version = mConfig.getValidVersion(url, mReferenceDate);
    }
    return DefaultFhirStructureRepositoryView::findValueSet(url, version);
}

const FhirCodeSystem* FhirKbvKeyTablesView::findCodeSystem(const std::string_view url,
                                                           std::optional<std::string> version) const
{
    if (! version)
    {
        version = mConfig.getValidVersion(url, mReferenceDate);
    }
    return DefaultFhirStructureRepositoryView::findCodeSystem(url, version);
}

}