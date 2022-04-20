/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONINGREDIENT_HXX

#include "erp/model/KbvMedicationBase.hxx"

namespace model
{

class KbvMedicationIngredient : public KbvMedicationBase<KbvMedicationIngredient, ResourceVersion::KbvItaErp>
{
public:
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
