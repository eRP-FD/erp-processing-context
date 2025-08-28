/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Resource.hxx"

namespace model
{

class GemErpEuPrPractitionerRole : public Resource<GemErpEuPrPractitionerRole>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PractitionerRole;

    static constexpr auto resourceTypeName = "PractitionerRole";

    GemErpEuPrPractitionerRole() = default;

    std::string_view practitionerReference() const;
    std::string_view organizationReference() const;

    void setPractitionerReference(std::string_view newReference);
    void setOrganizationReference(std::string_view newReference);

    friend class Resource;

private:
    explicit GemErpEuPrPractitionerRole(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrPractitionerRole>;
// NOLINTEND(bugprone-exception-escape)

}
