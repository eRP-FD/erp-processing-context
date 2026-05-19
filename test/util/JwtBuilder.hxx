/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_TOOLS_JWTBUILDER_HXX
#define ERP_TOOLS_JWTBUILDER_HXX

#include "shared/crypto/Jws.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/model/Kvnr.hxx"

#include <rapidjson/document.h>

#include <optional>

class JwtBuilder
{
public:
    explicit JwtBuilder(shared_EVP_PKEY key);
    explicit JwtBuilder(shared_EVP_PKEY key, JoseHeader header);

    [[nodiscard]]
	static std::string defaultClientId();

    [[nodiscard]]
    static JwtBuilder testBuilder();

    static JwtBuilder testBuilder(JoseHeader header);

    [[nodiscard]]
    JWT makeJwtApotheke(const std::string& telematikId = "");

    [[nodiscard]]
    JWT makeJwtArzt(const std::optional<std::string>& telematicId = std::nullopt);

    [[nodiscard]]
    JWT makeJwtVersicherter(const std::string& kvnr);

    [[nodiscard]]
    JWT makeJwtVersicherter(const model::Kvnr& kvnr);

    [[nodiscard]]
    JWT makeJwtKostentraeger(const std::string& telematikId = "");

    [[nodiscard]]
    JWT makeJwtNcpeh();


    struct JwtClaimsOptions {
        std::string acr = "gematik-ehealth-loa-high";
        std::string aud = "https://gematik.erppre.de/";
        std::string exp = "2524608000";
        std::string displayName = "Vorname Nachname";
        std::string iat = "1585336956";
        std::string idNummer = "X234567891";
        std::string iss = "https://idp1.telematik.de/jwt";
        std::string jti = "<IDP_01234567890123456789";
        std::string nbf = "1585336956";
        std::string nonce = "fuu bar baz";
        std::string orgName = "Institutions- oder Organisations-Bezeichnung";
        std::string profession = "1.2.276.0.76.4.49";
        std::string sub = "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk";
        std::string optionalFields = {};
    };
    [[nodiscard]]
    JWT makeJwt(const JwtClaimsOptions& options);

    [[nodiscard]]
    JWT getJWT(const rapidjson::Document& claims);

    [[nodiscard]]
    JWT getJWT(const std::string& claims);

    [[nodiscard]]
    JWT makeValidJwt(rapidjson::Document&& claims);

private:
    const shared_EVP_PKEY mKey;
    JoseHeader mJoseHeader;
};

#endif // ERP_TOOLS_JWTBUILDER_HXX
