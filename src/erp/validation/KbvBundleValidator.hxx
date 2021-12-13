/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvBundle;
}

class KbvBundleValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvBundleValidator_V1_0_1 : public KbvBundleValidator
{
protected:
    void doValidate(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

    void erp_compositionPflicht(const model::KbvBundle& kbvBundle) const;
    void erp_angabePruefnummer(const model::KbvBundle& kbvBundle) const;
    void erp_angabeZuzahlung(const model::KbvBundle& kbvBundle) const;
    void erp_angabePLZ(const model::KbvBundle& kbvBundle) const;
    void erp_angabeNrAusstellendePerson(const model::KbvBundle& kbvBundle) const;
    void erp_angabeBestriebsstaettennr(const model::KbvBundle& kbvBundle) const;
    void erp_angabeRechtsgrundlage(const model::KbvBundle& kbvBundle) const;
    void ikKostentraegerBgUkPflicht(const model::KbvBundle& kbvBundle) const;
    void validateMedicationProfiles(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                                    const InCodeValidator& inCodeValidator) const;
    void validateEntries(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                         const InCodeValidator& inCodeValidator,
                         const model::ResourceVersion::KbvItaErp& version) const;
    template<typename TResource>
    void validateEntry(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                       const InCodeValidator& inCodeValidator, SchemaType schemaType,
                       model::ResourceVersion::KbvItaErp version) const;

    bool isKostentraeger(const model::KbvBundle& kbvBundle,
                         std::set<std::string> kostentraeger = {"GKV", "BG", "SKT", "UK"}) const;
};

class KbvBundleValidator_V1_0_2 : public KbvBundleValidator_V1_0_1
{
protected:
    void doValidate(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX
