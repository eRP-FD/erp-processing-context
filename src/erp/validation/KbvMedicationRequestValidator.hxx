/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONREQUESTVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONREQUESTVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class Dosage;
class KbvMedicationRequest;
}


class KbvMedicationRequestValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvMedicationRequest& kbvMedicationRequest, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvMedicationRequestValidator_V1_0_2 : public KbvMedicationRequestValidator
{
protected:
    void doValidate(const model::KbvMedicationRequest& kbvMedicationRequest, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

    void erp_angabeDosierung(const model::Dosage& dosage) const;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONREQUESTVALIDATOR_HXX
