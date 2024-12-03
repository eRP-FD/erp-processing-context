/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_

#include "shared/model/Resource.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvOrganization : public Resource<KbvOrganization>
{
public:
    static constexpr auto resourceTypeName = "Organization";
    [[nodiscard]] bool identifierExists() const;
    [[nodiscard]] std::optional<std::string_view> telecom(const std::string_view& system) const;
    [[nodiscard]] std::optional<std::string_view> addressLine(size_t idx) const;
    [[nodiscard]] std::optional<model::UnspecifiedResource> address_line(size_t index) const;
    [[nodiscard]] std::optional<std::string_view> addressType() const;
    [[nodiscard]] std::optional<model::UnspecifiedResource> address() const;

private:
    friend Resource<KbvOrganization>;
    explicit KbvOrganization(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvOrganization>;
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_
