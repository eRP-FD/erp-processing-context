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

class GemErpEuPrOrganization : public Resource<GemErpEuPrOrganization>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_Organization;

    static constexpr auto resourceTypeName = "Organization";

    GemErpEuPrOrganization() = default;

    friend class Resource;

private:
    explicit GemErpEuPrOrganization(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrOrganization>;
// NOLINTEND(bugprone-exception-escape)

}
