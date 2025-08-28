// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "EuAccessPermission.hxx"
#include "shared/model/CountryCode.hxx"
#include "shared/model/Parameters.hxx"

namespace model {

class GemErpEuPrParAccessAuthorizationRequest : public Parameters<GemErpEuPrParAccessAuthorizationRequest> {
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PAR_Access_Authorization_Request;
    using Parameters::Parameters;
    friend class Resource;

    CountryCode getCountryCode() const;
    EuAccessCode getAccessCode() const;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrParAccessAuthorizationRequest>;
extern template class Parameters<GemErpEuPrParAccessAuthorizationRequest>;
// NOLINTEND
} // model
