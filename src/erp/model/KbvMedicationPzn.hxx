/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX

#include "shared/model/MedicationBase.hxx"
#include "shared/validation/SchemaType.hxx"
#include "erp/model/Pzn.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationPzn : public MedicationBase<KbvMedicationPzn>
{
public:
    static constexpr auto profileType = ProfileType::KBV_PR_ERP_Medication_PZN;

    [[nodiscard]] std::optional<std::string_view> amountNumeratorValueAsString() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorSystem() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorCode() const;
    [[nodiscard]] Pzn pzn() const;
    [[nodiscard]] bool isKPG() const;

private:
    friend Resource<KbvMedicationPzn>;
    explicit KbvMedicationPzn(NumberAsStringParserDocument&& document);
};


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationPzn>;
}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONPZN_HXX
