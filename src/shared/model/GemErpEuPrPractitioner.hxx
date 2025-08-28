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

class GemErpEuPrPractitioner : public Resource<GemErpEuPrPractitioner>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_Practitioner;

    static constexpr auto resourceTypeName = "Practitioner";

    std::string_view practitionerId() const;

    friend class Resource;

    GemErpEuPrPractitioner() = default;

private:
    explicit GemErpEuPrPractitioner(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrPractitioner>;
// NOLINTEND(bugprone-exception-escape)

}// model
