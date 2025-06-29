#ifndef ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX
#define ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX

#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/MedicationDispense.hxx"

#include <vector>

namespace model
{
class MedicationDispenseBundle;

class MedicationsAndDispenses {
public:
    void addFromBundle(const MedicationDispenseBundle& bundle);
    MedicationsAndDispenses filter(const MedicationDispenseId& medicationDispenseId) &&;

    bool containsDigaRedeemCode() const;

    std::vector<model::MedicationDispense> medicationDispenses{};
    std::vector<model::GemErpPrMedication> medications{};
};
}



#endif// ERP_PROCESSING_CONTEXT_MEDICATIONS_AND_DISPENSES_HXX
