/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_TOOLS_JWTBUILDER_HXX
#define ERP_TOOLS_JWTBUILDER_HXX

#include "erp/crypto/Jws.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/model/Kvnr.hxx"

#include <rapidjson/document.h>

#include <optional>

class JwtBuilder
{
public:
    explicit JwtBuilder(shared_EVP_PKEY key);

    [[nodiscard]]
	static std::string defaultClientId();

    [[nodiscard]]
    static JwtBuilder testBuilder();

    [[nodiscard]]
    JWT makeJwtApotheke(const std::optional<std::string>& telematicId = std::nullopt);

    [[nodiscard]]
    JWT makeJwtArzt(const std::optional<std::string>& telematicId = std::nullopt);

    [[nodiscard]]
    JWT makeJwtVersicherter(const std::string& kvnr);

    [[nodiscard]]
    JWT makeJwtVersicherter(const model::Kvnr& kvnr);

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
