/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPATIENTVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPATIENTVALIDATOR_HXX

#include "erp/validation/InCodeValidator.hxx"

#include <optional>

namespace model
{
class Patient;
class UnspecifiedResource;
class Reference;
}


class KbvPatientValidator : public ResourceValidator
{
public:
    void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                  const InCodeValidator& inCodeValidator) const override;

protected:
    virtual void doValidate(const model::Patient& patient, const XmlValidator& xmlValidator,
                            const InCodeValidator& inCodeValidator) const = 0;
};

class KbvPatientValidator_V1_0_2 : public KbvPatientValidator
{
protected:
    void doValidate(const model::Patient& patient, const XmlValidator& xmlValidator,
                    const InCodeValidator& inCodeValidator) const override;

    void identifierSlicing(const model::Patient& patient) const;
    void addressSlicing(const model::Patient& patient) const;


    void identifierSlicingGKV(const std::optional<std::string_view>& identifierUse,
                              const std::optional<std::string_view>& identifierTypeCodingSystem,
                              const std::optional<std::string_view>& identifierSystem,
                              const std::optional<std::string_view>& identifierValue) const;
    void identifierSlicingPKV(const std::optional<std::string_view>& identifierUse,
                              const std::optional<std::string_view>& identifierTypeCodingSystem) const;
    void identifierSlicingKvk(const std::optional<std::string_view>& identifierUse,
                              const std::optional<std::string_view>& identifierTypeCodingSystem,
                              const std::optional<std::string_view>& identifierSystem) const;
    void pkvAssigner(const std::optional<model::Reference>& assigner) const;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVPATIENTVALIDATOR_HXX
