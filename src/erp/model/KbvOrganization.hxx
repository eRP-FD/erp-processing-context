/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_

#include "erp/model/Resource.hxx"

namespace model
{

class KbvOrganization : public Resource<KbvOrganization, ResourceVersion::KbvItaErp>
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
    friend Resource<KbvOrganization, ResourceVersion::KbvItaErp>;
    explicit KbvOrganization(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVORGANIZATION_HXX_
