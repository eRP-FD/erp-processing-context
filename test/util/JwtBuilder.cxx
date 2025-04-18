/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/JwtBuilder.hxx"

#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Base64.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "ResourceManager.hxx"

#include <chrono>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


namespace
{

void setIdNummer(rapidjson::Document& claims, const std::string& id)
{
    static const rapidjson::Pointer idNummer{"/idNummer"};
    idNummer.Set(claims, id);
}

}

std::string JwtBuilder::defaultClientId()
{
    return "erp-testsuite-fd";
}

JwtBuilder::JwtBuilder(shared_EVP_PKEY key)
    : mKey(std::move(key))
    , mJoseHeader(JoseHeader::Algorithm::BP256R1)
{
}

JwtBuilder JwtBuilder::testBuilder()
{
    return JwtBuilder{MockCryptography::getIdpPrivateKey()};
}

JWT JwtBuilder::getJWT(const rapidjson::Document& claims)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    Expect(claims.Accept(writer), "Failed to serialize claim.");

    const Jws jws(mJoseHeader, buffer.GetString(), mKey);
    return JWT(jws.compactSerialized());
}

JWT JwtBuilder::getJWT(const std::string& claims)
{
    const Jws jws(mJoseHeader, claims, mKey);
    return JWT(jws.compactSerialized());
}

JWT JwtBuilder::makeValidJwt(rapidjson::Document&& claims)
{
    using namespace std::chrono_literals;
    static const rapidjson::Pointer exp{"/exp"};
    static const rapidjson::Pointer iat{"/iat"};
    static const rapidjson::Pointer aud{"/aud"};
    exp.Set(claims, int64_t(std::chrono::duration_cast<std::chrono::seconds>(
        (std::chrono::system_clock::now() + std::chrono::minutes (5)).time_since_epoch()).count()));
    iat.Set(claims, int64_t(std::chrono::duration_cast<std::chrono::seconds>(
        (std::chrono::system_clock::now() - std::chrono::minutes(1)).time_since_epoch()).count()));
    const std::string audContent = Configuration::instance().getOptionalStringValue(ConfigurationKey::IDP_REGISTERED_FD_URI, "");
    if (!audContent.empty())
    {
        aud.Set(claims, audContent);
    }
    claims.RemoveMember(std::string{JWT::nbfClaim});
    if (!claims.HasMember(std::string{JWT::clientIdClaim}))
    {
        claims.AddMember(rapidjson::Value{std::string{JWT::clientIdClaim}, claims.GetAllocator()},
                         rapidjson::Value{JwtBuilder::defaultClientId(), claims.GetAllocator()}, claims.GetAllocator());
    }
    return getJWT(claims);
}

JWT JwtBuilder::makeJwtApotheke(const std::optional<std::string>& telematicId)
{
    static auto constexpr templateResource = "test/jwt/claims_apotheke.json";
    auto& resourceManager = ResourceManager::instance();
    const auto& claimTemplate = resourceManager.getJsonResource(templateResource);
    rapidjson::Document claims;
    claims.CopyFrom(claimTemplate, claims.GetAllocator());
    if(telematicId.has_value())
        setIdNummer(claims, telematicId.value());
    return makeValidJwt(std::move(claims));
}

JWT JwtBuilder::makeJwtArzt(const std::optional<std::string>& telematicId)
{
    static auto constexpr templateResource = "test/jwt/claims_arzt.json";
    auto& resourceManager = ResourceManager::instance();
    const auto& claimTemplate = resourceManager.getJsonResource(templateResource);
    rapidjson::Document claims;
    claims.CopyFrom(claimTemplate, claims.GetAllocator());
    if(telematicId.has_value())
        setIdNummer(claims, telematicId.value());
    return makeValidJwt(std::move(claims));
}

JWT JwtBuilder::makeJwtVersicherter(const std::string& kvnr)
{
    static auto constexpr templateResource = "test/jwt/claims_patient.json";
    auto& resourceManager = ResourceManager::instance();
    const auto& claimTemplate = resourceManager.getJsonResource(templateResource);
    rapidjson::Document claims;
    claims.CopyFrom(claimTemplate, claims.GetAllocator());
    setIdNummer(claims, kvnr);
    return makeValidJwt(std::move(claims));
}

JWT JwtBuilder::makeJwtVersicherter(const model::Kvnr& kvnr)
{
    return makeJwtVersicherter(kvnr.id());
}

JWT JwtBuilder::makeJwtKostentraeger(const std::optional<std::string>& telematicId)
{
    static auto constexpr templateResource = "test/jwt/claims_kostentraeger.json";
    auto& resourceManager = ResourceManager::instance();
    const auto& claimTemplate = resourceManager.getJsonResource(templateResource);
    rapidjson::Document claims;
    claims.CopyFrom(claimTemplate, claims.GetAllocator());
    if (telematicId.has_value())
    {
        setIdNummer(claims, telematicId.value());
    }
    return makeValidJwt(std::move(claims));
}
