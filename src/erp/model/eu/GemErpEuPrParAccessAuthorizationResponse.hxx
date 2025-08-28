// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/model/Parameters.hxx"

namespace model
{
class EuAccessPermission;
class CountryCode;
class Kvnr;

class GemErpEuPrParAccessAuthorizationResponse : public Parameters<GemErpEuPrParAccessAuthorizationResponse> {
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PAR_Access_Authorization_Response;
    using Parameters::Parameters;
    friend class Resource;

    GemErpEuPrParAccessAuthorizationResponse(const EuAccessPermission& accessPermission, const Kvnr& kvnr);
};


// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrParAccessAuthorizationResponse>;
extern template class Parameters<GemErpEuPrParAccessAuthorizationResponse>;
// NOLINTEND
} // model
