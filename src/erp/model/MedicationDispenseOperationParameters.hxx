#ifndef ERP_PROCESSING_CONTEXT_MEDICATIONDISPENSEOPERATIONPARAMETERS_HXX
#define ERP_PROCESSING_CONTEXT_MEDICATIONDISPENSEOPERATIONPARAMETERS_HXX

#include "shared/model/Parameters.hxx"

#include <list>

namespace model {

class GemErpPrMedication;
class MedicationDispense;

/// @brief Model for both GEM_ERP_PR_PAR_CloseOperation_Input and GEM_ERP_PR_PAR_DispenseOperation_Input
///
class MedicationDispenseOperationParameters : public model::Parameters<MedicationDispenseOperationParameters>
{
public:
    using Parameters::Parameters;

    model::ProfileType profileType() const;

    gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> getValidationView() const override;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;

    std::list<std::pair<MedicationDispense, GemErpPrMedication>> medicationDispenses() const;

private:
    friend class Resource<MedicationDispenseOperationParameters>;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<MedicationDispenseOperationParameters>;
extern template class Parameters<MedicationDispenseOperationParameters>;
// NOLINTEND(bugprone-exception-escape)
}


#endif // ERP_PROCESSING_CONTEXT_MEDICATIONDISPENSEOPERATIONPARAMETERS_HXX
