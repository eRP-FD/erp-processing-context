/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONDUMMY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONDUMMY_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationDummy : public KbvMedicationBase<KbvMedicationDummy, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_BundleDummy;

private:
    friend Resource<KbvMedicationDummy, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationDummy(NumberAsStringParserDocument&& document);
};

}// namespace model


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONDUMMY_HXX
