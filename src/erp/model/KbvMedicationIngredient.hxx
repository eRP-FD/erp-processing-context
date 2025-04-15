/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX

#include "shared/model/MedicationBase.hxx"
#include "shared/model/ProfileType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationIngredient : public MedicationBase<KbvMedicationIngredient>
{
public:
    static constexpr auto profileType = ProfileType::KBV_PR_ERP_Medication_Ingredient;

    [[nodiscard]] std::optional<std::string_view> amountNumeratorValueAsString() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorSystem() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorCode() const;
    [[nodiscard]] std::optional<std::string_view> ingredientStrengthNumeratorValueAsString() const;
private:
    friend Resource<KbvMedicationIngredient>;
    explicit KbvMedicationIngredient(NumberAsStringParserDocument&& document);
};


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationIngredient>;
}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX
