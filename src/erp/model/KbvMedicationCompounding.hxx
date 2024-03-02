/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/Pzn.hxx"
#include "erp/validation/SchemaType.hxx"

#include <vector>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationCompounding : public KbvMedicationBase<KbvMedicationCompounding, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_Compounding;
    const rapidjson::Value* ingredientArray() const;
    std::vector<Pzn> ingredientPzns() const;

private:
    friend Resource<KbvMedicationCompounding, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationCompounding(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
