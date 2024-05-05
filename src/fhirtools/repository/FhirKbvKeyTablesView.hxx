/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_REPOSITORY_FHIRRESOURCEVIEW_H
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_REPOSITORY_FHIRRESOURCEVIEW_H

#include "FhirResourceViewConfiguration.hxx"
#include "erp/model/Timestamp.hxx"
#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"


namespace fhirtools
{

class FhirKbvKeyTablesView : public DefaultFhirStructureRepositoryView
{
public:
    explicit FhirKbvKeyTablesView(gsl::not_null<const FhirStructureRepositoryBackend*> backend,
                                  const FhirResourceViewConfiguration& config, const Date& referenceDate);

    const FhirValueSet* findValueSet(const std::string_view url, std::optional<std::string> version) const override;
    const FhirCodeSystem* findCodeSystem(const std::string_view url, std::optional<std::string> version) const override;

private:
    FhirResourceViewConfiguration mConfig;
    fhirtools::Date mReferenceDate;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_REPOSITORY_FHIRRESOURCEVIEW_H
