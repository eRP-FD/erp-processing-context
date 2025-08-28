// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "EuAccessPermission.hxx"
#include "shared/model/CountryCode.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/util/Uuid.hxx"

namespace model
{

GemErpEuPrParAccessAuthorizationResponse::GemErpEuPrParAccessAuthorizationResponse(const EuAccessPermission& accessPermission, const Kvnr& kvnr)
    : Parameters(ProfileType::GEM_ERPEU_PR_PAR_Access_Authorization_Response)
{
    setResourceType("Parameters");
    setId("eu-access-permission-" + kvnr.id() + "-" + accessPermission.getCountryCode().toString());

    auto& countryCodeParameter = findOrAddParameter("countryCode");
    setKeyValue(countryCodeParameter, valueCodingSystemPointer, resource::naming_system::iso3166);
    setKeyValue(countryCodeParameter, valueCodingCodePointer, accessPermission.getCountryCode().toString());

    auto& accessCodeParameter = findOrAddParameter("accessCode");
    setKeyValue(accessCodeParameter, valueIdentifierSystemPointer, resource::naming_system::euAccessCode);
    setKeyValue(accessCodeParameter, valueIdentifierValuePointer, accessPermission.getAccessCode().toString());

    setKeyValue(findOrAddParameter("validUntil"), valueInstantPointer, accessPermission.getValidUntil().toXsDateTime());
    setKeyValue(findOrAddParameter("createdAt"), valueInstantPointer, accessPermission.getCreatedAt().toXsDateTime());
}

template class Parameters<GemErpEuPrParAccessAuthorizationResponse>;
}