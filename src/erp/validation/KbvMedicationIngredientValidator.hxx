/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONINGREDIENTVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONINGREDIENTVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvMedicationIngredient;
}


class KbvMedicationIngredientValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvMedicationIngredient& kbvMedicationCompounding,
                            const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator) const = 0;
};

class KbvMedicationIngredientValidator_V1_0_2 : public KbvMedicationIngredientValidator
{
protected:
    void doValidate(const model::KbvMedicationIngredient& kbvMedicationCompounding, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

private:
    void erp_PackungOderNormgroesse(const model::KbvMedicationIngredient& kbvMedicationIngredient) const;
    void amountNumerator_erp_begrenzungValue(const model::KbvMedicationIngredient& kbvMedicationIngredient) const;
    void amountNumerator_erp_codeUndSystem(const model::KbvMedicationIngredient& kbvMedicationIngredient) const;
    void ingredientStrengthNumerator_erp_begrenzungValue(
        const model::KbvMedicationIngredient& kbvMedicationIngredient) const;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONINGREDIENTVALIDATOR_HXX
