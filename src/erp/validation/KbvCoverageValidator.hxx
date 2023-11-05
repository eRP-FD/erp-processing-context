/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOVERAGEVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOVERAGEVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvCoverage;
}

class KbvCoverageValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

    virtual void doValidate(const model::KbvCoverage& kbvCoverage, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvCoverageValidator_V1_0_2 : public KbvCoverageValidator
{
public:
    void doValidate(const model::KbvCoverage& kbvCoverage, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

private:
    void IK_zustaendige_Krankenkasse_eGK(const model::KbvCoverage& kbvCoverage) const;
    void Versichertenart_Pflicht(const model::KbvCoverage& kbvCoverage) const;
    void Besondere_Personengruppe_Pflicht(const model::KbvCoverage& kbvCoverage) const;
    void IK_Kostentraeger_BG_UK(const model::KbvCoverage& kbvCoverage) const;
    void DMP_Pflicht(const model::KbvCoverage& kbvCoverage) const;
    void KBVdate(const model::KbvCoverage& kbvCoverage) const;
};

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVCOVERAGEVALIDATOR_HXX
