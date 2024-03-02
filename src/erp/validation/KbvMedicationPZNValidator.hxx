/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONPZNVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONPZNVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvMedicationPzn;
}


class KbvMedicationPZNValidator: public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;
protected:
    virtual void doValidate(const model::KbvMedicationPzn& kbvMedicationpzn,
                            const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator) const = 0;
};

class KbvMedicationPZNValidator_V1_0_2 : public KbvMedicationPZNValidator
{
protected:
    void doValidate(const model::KbvMedicationPzn& kbvMedicationpzn, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

private:
    void erp_NormgroesseOderMenge(const model::KbvMedicationPzn& kbvMedicationpzn) const;
    void amountNumerator_erp_begrenzungValue(const model::KbvMedicationPzn& kbvMedicationPzn) const;
    void amountNumerator_erp_codeUndSystem(const model::KbvMedicationPzn& kbvMedicationPzn) const;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONPZNVALIDATOR_HXX
