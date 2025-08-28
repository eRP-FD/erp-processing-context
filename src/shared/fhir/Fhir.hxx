/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "shared/util/Configuration.hxx"

#include <list>
#include <string_view>

namespace model
{
class Timestamp;
}

namespace fhirtools
{
class FhirStructureRepositoryBackend;
class FhirResourceViewList;
class ValidatorOptions;
}

class FhirConverter;

class Fhir {
public:
    enum class Init
    {
        now,
        later,
    };

    template<config::ProcessType ProcessT>
    static void init(Init init = Init::now);

    static const Fhir& instance();

    virtual const FhirConverter& converter() const = 0;

    virtual fhirtools::FhirResourceViewList allViews() const = 0;

    virtual fhirtools::FhirResourceViewList structureRepository(const model::Timestamp& referenceTimestamp) const = 0;

    virtual const fhirtools::FhirStructureRepositoryBackend& backend() const = 0;

    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> defaultView() const;

    virtual fhirtools::FhirResourceViewConfiguration fhirResourceViewConfiguration() const = 0;

    virtual fhirtools::ValidatorOptions defaultValidatorOptions(model::ProfileType profileType,
                                                                const model::Timestamp& referenceTimestamp) const = 0;

    virtual ~Fhir() = 0;
private:
    virtual void ensureInitialized() = 0;

    static std::unique_ptr<Fhir> mInstance;

};

extern template void Fhir::init<ConfigurationBase::ERP>(Init init);
extern template void Fhir::init<ConfigurationBase::MedicationExporter>(Init init);


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX
