/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVORGANIZATIONVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVORGANIZATIONVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvOrganization;
}

class KbvOrganizationValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
        virtual void doValidate(const model::KbvOrganization& kbvOrganization, const XmlValidator& xmlValidator,
                                const InCodeValidator& inCodeValidator) const = 0;
};

class KbvOrganizationValidator_V1_0_1 : public KbvOrganizationValidator
{
protected:
    void doValidate(const model::KbvOrganization& kbvOrganization, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

    void identifierSlicing(const model::KbvOrganization& kbvOrganization) const;
    void telecomSlicing(const model::KbvOrganization& kbvOrganization) const;
    void addressExtensions(const model::KbvOrganization& kbvOrganization) const;
};

using KbvOrganizationValidator_V1_0_2 = KbvOrganizationValidator_V1_0_1;


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVORGANIZATIONVALIDATOR_HXX
