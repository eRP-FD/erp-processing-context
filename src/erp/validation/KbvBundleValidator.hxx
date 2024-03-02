/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

namespace model
{
class KbvBundle;
}

class KbvBundleValidator_V1_0_2 : public ResourceValidator
{
protected:
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
                         const InCodeValidator& inCodeValidator) const;
    template<typename TResource>
    void validateEntry(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                       const InCodeValidator& inCodeValidator, SchemaType schemaType) const;

    bool isKostentraeger(const model::KbvBundle& kbvBundle,
                         std::set<std::string> kostentraeger = {"GKV", "BG", "SKT", "UK"}) const;

    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                          const InCodeValidator& inCodeValidator) const override;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVBUNDLEVALIDATOR_HXX
