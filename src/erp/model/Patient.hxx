/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PATIENT_HXX
#define ERP_PROCESSING_CONTEXT_PATIENT_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/Kvnr.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class Patient : public Resource<Patient, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "Patient";
    [[nodiscard]] Kvnr kvnr() const;
    [[nodiscard]] std::optional<std::string_view> postalCode() const;

    [[nodiscard]] std::optional<UnspecifiedResource> birthDate() const;
    [[nodiscard]] std::optional<std::string_view> nameFamily() const;
    [[nodiscard]] std::optional<UnspecifiedResource> name_family() const;
    [[nodiscard]] std::optional<std::string_view> namePrefix() const;
    [[nodiscard]] std::optional<UnspecifiedResource> name_prefix() const;

    [[nodiscard]] std::optional<std::string_view> addressType() const;
    [[nodiscard]] std::optional<std::string_view> addressLine(size_t idx) const;
    [[nodiscard]] std::optional<UnspecifiedResource> address_line(size_t idx) const;
    [[nodiscard]] model::UnspecifiedResource address() const;

    [[nodiscard]] std::optional<std::string_view> identifierAssignerDisplay() const;
    [[nodiscard]] std::optional<model::Reference> identifierAssigner() const;
    [[nodiscard]] bool hasIdentifier() const;

private:
    friend Resource<Patient, ResourceVersion::KbvItaErp>;
    explicit Patient (NumberAsStringParserDocument&& document);

};

}

#endif //ERP_PROCESSING_CONTEXT_PATIENT_HXX
