/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOMPOSITIONVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOMPOSITIONVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvComposition;
}

class KbvCompositionValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvComposition& kbvComposition, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvCompositionValidator_V1_0_1 : public KbvCompositionValidator
{
private:
    void doValidate(const model::KbvComposition& kbvComposition, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

    void erp_subjectAndPrescription(const model::KbvComposition& kbvComposition) const;
    void erp_coverageAndPrescription(const model::KbvComposition& kbvComposition) const;
    void erp_prescriptionOrPracticeSupply(const model::KbvComposition& kbvComposition) const;

    void authorSlicing(const model::KbvComposition& kbvComposition) const;
    void checkAuthor(const model::KbvComposition& kbvComposition, const std::string_view authorType, size_t idx) const;
};

using KbvCompositionValidator_V1_0_2 = KbvCompositionValidator_V1_0_1;


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOMPOSITIONVALIDATOR_HXX
