/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationIngredient : public KbvMedicationBase<KbvMedicationIngredient, ResourceVersion::KbvItaErp>
{
public:
    static constexpr SchemaType schemaType = SchemaType::KBV_PR_ERP_Medication_Ingredient;

    [[nodiscard]] std::optional<std::string_view> amountNumeratorValueAsString() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorSystem() const;
    [[nodiscard]] std::optional<std::string_view> amountNumeratorCode() const;
    [[nodiscard]] std::optional<std::string_view> ingredientStrengthNumeratorValueAsString() const;
private:
    friend Resource<KbvMedicationIngredient, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationIngredient(NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX
