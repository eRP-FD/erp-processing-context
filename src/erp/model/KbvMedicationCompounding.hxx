/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationCompounding : public KbvMedicationBase<KbvMedicationCompounding, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_Compounding;
    const rapidjson::Value* ingredientArray() const;

private:
    friend Resource<KbvMedicationCompounding, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationCompounding(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONCOMPOUNDING_HXX
