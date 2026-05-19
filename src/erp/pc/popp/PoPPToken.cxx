/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPToken.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Expect.hxx"

#include <fmt/format.h>


PoPPToken::PoPPToken(const std::string& jwt)
    : JWT(jwt)
    , mHeaderDocument(headerDocument())
{
}

std::string_view PoPPToken::typ() const
{
    return getClaim(mHeaderDocument, typClaim);
}

std::string_view PoPPToken::alg() const
{
    return getClaim(mHeaderDocument, algClaim);
}

std::string_view PoPPToken::kid() const
{
    return getClaim(mHeaderDocument, kidClaim);
}

std::string_view PoPPToken::iss() const
{
    return getClaim(claimsDocument(), JWT::issClaim);
}

model::Timestamp PoPPToken::iat() const
{
    const auto& value = getClaimValue(claimsDocument(), JWT::iatClaim);
    ModelExpect(value.IsInt64(), "iat claim is not an int64");
    return model::Timestamp{value.GetInt64()};
}

std::string_view PoPPToken::actorId() const
{
    return getClaim(claimsDocument(), actorIdClaim);
}

model::Kvnr PoPPToken::kvnr() const
{
    return model::Kvnr{getClaim(claimsDocument(), patientIdClaim)};
}

PoPPTokenProofMethod PoPPToken::proofMethod() const
{
    auto proofMethodStr = getClaim(claimsDocument(), proofMethodClaim);
    auto enumOpt = magic_enum::enum_cast<PoPPTokenProofMethod>(proofMethodStr);
    ModelExpect(enumOpt.has_value(), fmt::format("unknown proofMethod claim '{}' in PoPP-Token", proofMethodStr));
    return enumOpt.value();
}

PoPPTokenProofMethodPrefix PoPPToken::proofMethodPrefix() const
{
    auto proofMethodStr = getClaim(claimsDocument(), proofMethodClaim);
    if (proofMethodStr == "healthid")
    {
        return PoPPTokenProofMethodPrefix::healthid;
    }
    if (proofMethodStr.starts_with("ehc-practitioner-"))
    {
        return PoPPTokenProofMethodPrefix::ehc_practitioner;
    }
    if (proofMethodStr.starts_with("ehc-provider-"))
    {
        return PoPPTokenProofMethodPrefix::ehc_provider;
    }
    TLOG(WARNING) << "Unknown proof method " << proofMethodStr << " in PoPP-Token";
    return PoPPTokenProofMethodPrefix::unknown;
}

const rapidjson::Value& PoPPToken::getClaimValue(const rapidjson::Document& document, std::string_view claimName)
{
    const auto it = document.FindMember(rapidjson::StringRef(claimName.data(), claimName.size()));
    ModelExpect(it != document.MemberEnd(), fmt::format("missing claim field: {}", claimName));
    return it->value;
}

std::string_view PoPPToken::getClaim(const rapidjson::Document& document, std::string_view claimName)
{
    const auto& value = getClaimValue(document, claimName);
    ModelExpect(value.IsString(), fmt::format("claim field {} is not a string", claimName));
    return value.GetString();
}
