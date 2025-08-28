// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParAccessAuthorizationRequest.hxx"

namespace model
{

CountryCode GemErpEuPrParAccessAuthorizationRequest::getCountryCode() const
{
    const auto* countryCodeParameter = findParameter("countryCode");
    ModelExpect(countryCodeParameter,
                "countryCode parameter not found in GEM_ERPEU_PR_PAR_Access_Authorization_Request");
    auto countryCode = getOptionalStringValue(*countryCodeParameter, valueCodingCodePointer);
    ModelExpect(countryCode, "countryCode parameter not found in GEM_ERPEU_PR_PAR_Access_Authorization_Request");
    return CountryCode{std::string{*countryCode}};
}

EuAccessCode GemErpEuPrParAccessAuthorizationRequest::getAccessCode() const
{
    const auto* accessCodeParameter = findParameter("accessCode");
    ModelExpect(accessCodeParameter, "accessCode parameter not found in GEM_ERPEU_PR_PAR_Access_Authorization_Request");
    auto accessCode = getOptionalStringValue(*accessCodeParameter, valueIdentifierValuePointer);
    ModelExpect(accessCode, "accessCode parameter not found in GEM_ERPEU_PR_PAR_Access_Authorization_Request");
    return EuAccessCode{SafeString{std::string{*accessCode}}};
}

template class Parameters<GemErpEuPrParAccessAuthorizationRequest>;
}