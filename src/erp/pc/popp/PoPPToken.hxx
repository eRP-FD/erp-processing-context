/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/crypto/Jwt.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PoPPTokenProofMethod.hxx"

namespace model
{
class Timestamp;
}

class PoPPToken : public JWT
{
public:
    constexpr static std::string_view typClaim = "typ";
    constexpr static std::string_view algClaim = "alg";
    constexpr static std::string_view kidClaim = "kid";
    constexpr static std::string_view actorIdClaim = "actorId";
    constexpr static std::string_view patientIdClaim = "patientId";
    constexpr static std::string_view proofMethodClaim = "proofMethod";

    static constexpr std::string_view typHeader = "vnd.telematik.popp+jwt";
    static constexpr std::string_view algHeader = "ES256";

    explicit PoPPToken(const std::string& jwt);

    // header fields
    [[nodiscard]] std::string_view typ() const;
    [[nodiscard]] std::string_view alg() const;
    [[nodiscard]] std::string_view kid() const;

    // payload fields
    [[nodiscard]] std::string_view iss() const;
    [[nodiscard]] model::Timestamp iat() const;
    [[nodiscard]] std::string_view actorId() const;
    [[nodiscard]] model::Kvnr kvnr() const;
    [[nodiscard]] PoPPTokenProofMethod proofMethod() const;
    [[nodiscard]] PoPPTokenProofMethodPrefix proofMethodPrefix() const;

private:
    static const rapidjson::Value& getClaimValue(const rapidjson::Document& document, std::string_view claimName);
    static std::string_view getClaim(const rapidjson::Document& document, std::string_view claimName);

    rapidjson::Document mHeaderDocument;
};
