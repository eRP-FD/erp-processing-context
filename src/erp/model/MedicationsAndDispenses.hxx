/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX
#define ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX

#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/MedicationDispense.hxx"

#include <vector>

namespace model
{
class MedicationDispenseOperationParameters;
class MedicationDispenseBundle;

class MedicationsAndDispenses {
public:
    void addFromBundle(const MedicationDispenseBundle& bundle);
    void addFromDigaParameters(const model::MedicationDispenseOperationParameters& digaParameters);
    void addFromParameters(const model::MedicationDispenseOperationParameters& parameters);
    MedicationsAndDispenses filter(const MedicationDispenseId& medicationDispenseId) &&;

    bool containsDigaRedeemCode() const;

    std::vector<model::MedicationDispense> medicationDispenses{};
    std::vector<model::GemErpPrMedication> medications{};
};
}



#endif// ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX
