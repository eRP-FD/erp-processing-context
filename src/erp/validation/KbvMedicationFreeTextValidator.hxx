/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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

class KbvMedicationFreeTextValidator_V1_0_1 : public KbvMedicationFreeTextValidator
{
protected:
    void doValidate(const model::KbvMedicationFreeText& kbvMedicationCompounding, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;
};

using KbvMedicationFreeTextValidator_V1_0_2 = KbvMedicationFreeTextValidator_V1_0_1;


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONFREETEXTVALIDATOR_HXX
