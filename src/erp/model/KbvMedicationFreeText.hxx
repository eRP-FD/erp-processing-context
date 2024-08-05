/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/ProfileType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationFreeText : public KbvMedicationBase<KbvMedicationFreeText>
{
public:
    static constexpr auto schemaType = ProfileType::KBV_PR_ERP_Medication_FreeText;

private:
    friend Resource<KbvMedicationFreeText>;
    explicit KbvMedicationFreeText(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationFreeText>;
}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX
