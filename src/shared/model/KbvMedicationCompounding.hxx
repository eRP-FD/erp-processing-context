/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX

#include "shared/model/MedicationBase.hxx"
#include "shared/model/ProfileType.hxx"
#include "shared/model/Pzn.hxx"

#include <vector>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationCompounding : public MedicationBase<KbvMedicationCompounding>
{
public:
    static constexpr auto profileType = ProfileType::KBV_PR_ERP_Medication_Compounding;
    const rapidjson::Value* ingredientArray() const;
    std::vector<Pzn> ingredientPzns() const;

private:
    friend Resource<KbvMedicationCompounding>;
    explicit KbvMedicationCompounding(NumberAsStringParserDocument&& document);
};


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationCompounding>;
}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
