/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONCOMPOUNDINGVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONCOMPOUNDINGVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvMedicationCompounding;
}

class KbvMedicationCompoundingValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvMedicationCompounding& kbvMedicationCompounding,
                            const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator) const = 0;
};

class KbvMedicationCompoundingValidator_V1_0_2 : public KbvMedicationCompoundingValidator
{
protected:
    void doValidate(const model::KbvMedicationCompounding& kbvMedicationCompounding, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONCOMPOUNDINGVALIDATOR_HXX
