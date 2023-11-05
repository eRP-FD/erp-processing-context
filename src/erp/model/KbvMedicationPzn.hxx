/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/validation/SchemaType.hxx"
#include "erp/model/Pzn.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationPzn : public KbvMedicationBase<KbvMedicationPzn, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_PZN;

    [[nodiscard]] std::optional<std::string_view> amountNumeratorValueAsString() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorSystem() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorCode() const;
    [[nodiscard]] Pzn pzn() const;

private:
    friend Resource<KbvMedicationPzn, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationPzn(NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
