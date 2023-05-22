/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONFREETEXTVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONFREETEXTVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvMedicationFreeText;
}

class KbvMedicationFreeTextValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;
protected:
    virtual void doValidate(const model::KbvMedicationFreeText& kbvMedicationCompounding,
                            const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator) const = 0;
};

class KbvMedicationFreeTextValidator_V1_0_2 : public KbvMedicationFreeTextValidator
{
protected:
    void doValidate(const model::KbvMedicationFreeText& kbvMedicationCompounding, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONFREETEXTVALIDATOR_HXX
