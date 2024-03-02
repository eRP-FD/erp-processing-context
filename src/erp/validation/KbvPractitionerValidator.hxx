/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTITIONERVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTITIONERVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvPractitioner;
}

class KbvPractitionerValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvPractitioner& kbvPractitioner) const = 0;
};

class KbvPractitionerValidator_V1_0_2 : public KbvPractitionerValidator
{
protected:
    void doValidate(const model::KbvPractitioner& kbvPractitioner) const override;

    void identifierSlicing(const model::KbvPractitioner& kbvPractitioner) const;
    void qualificationSlicing(const model::KbvPractitioner& kbvPractitioner) const;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPRACTITIONERVALIDATOR_HXX
