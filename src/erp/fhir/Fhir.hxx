/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "erp/fhir/FhirConverter.hxx"
#include "fhirtools/repository/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <list>
#include <string_view>

namespace model
{
class Timestamp;
}

class FhirConverter;

class Fhir final
{
public:
    static const Fhir& instance();

    const FhirConverter& converter() const
    {
        return mConverter;
    }

    fhirtools::FhirResourceViewConfiguration::ViewList
    structureRepository(const model::Timestamp& referenceTimestamp) const;

    const fhirtools::FhirStructureRepositoryBackend& backend() const
    {
        return mBackend;
    }

private:
    Fhir();
    void load(ConfigurationKey structureKey);
    void validateViews() const;

    FhirConverter mConverter;
    fhirtools::FhirStructureRepositoryBackend mBackend;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX
