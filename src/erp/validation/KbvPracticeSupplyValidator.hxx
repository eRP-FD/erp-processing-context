/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTICESUPPLYVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTICESUPPLYVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvPracticeSupply;
}



class KbvPracticeSupplyValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvPracticeSupply& kbvPracticeSupply, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvPracticeSupplyValidator_V1_0_1 : public KbvPracticeSupplyValidator
{
protected:
    void doValidate(const model::KbvPracticeSupply& kbvPracticeSupply, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;
};

using KbvPracticeSupplyValidator_V1_0_2 = KbvPracticeSupplyValidator_V1_0_1;


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTICESUPPLYVALIDATOR_HXX
