/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "tools/jwt/JwtBuilder.hxx"

#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Base64.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "tools/ResourceManager.hxx"

#include <chrono>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


namespace
{

void setIdNummer(rapidjson::Document& claims, const std::string id)
{
    static const rapidjson::Pointer idNummer{"/idNummer"};
    idNummer.Set(claims, id);
}

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
    if (audContent.length())
    {
        aud.Set(claims, audContent);
    }
    claims.RemoveMember(std::string{JWT::nbfClaim});
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
