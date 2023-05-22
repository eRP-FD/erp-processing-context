/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationFreeText : public KbvMedicationBase<KbvMedicationFreeText, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_FreeText;

private:
    friend Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationFreeText(NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX
