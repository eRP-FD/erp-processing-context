/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_JWT_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_JWT_HXX

#include "erp/crypto/OpenSslHelper.hxx"

#include <memory>
#include <optional>
#include <rapidjson/document.h>
#include <string>


class SafeString;


/**
 * Representation of a JWT (Json Web Token) as it is used by eRp.
 *
 * Implementation and data model are not yet implemented.
 * What is currently there is a stand-in and will be replaced later.
 */
class JWT
{
public:
    JWT(void) = default;
    JWT(const JWT& other);

    /**
     * Pass a JWT as expected (b64EncHeader.b64EncPayload.b64EncSignature). The claims are
     * initialized with the payload and can be queried afterwards.
     */
    explicit JWT(std::string jwt);

    // NOLINTNEXTLINE(bugprone-exception-escape)
    JWT& operator=(JWT&& other) noexcept = default;

    bool operator==(const JWT& jwt) const;

    /**
     * Return a representation in "JWS Compact Serialization", according to RFC 7515 7 and 7.1.
     */
    std::string serialize(void) const;

    /**
     * Check pre-conditions and call verifySignature. This method should be used for
     * a complete verification. It includes the expiration and issued-at (iat) checks.
     *
     * Throws JwtException
     */
    void verify(const shared_EVP_PKEY& publicKey) const;

    /**
     * Throw an exception if the JWT can not be verified.
     *
     * Throws JwtException
     */
    void verifySignature(const shared_EVP_PKEY& publicKey) const;

    std::optional<std::int64_t> intForClaim(const std::string_view& claimName) const;
    std::optional<double> doubleForClaim(const std::string_view& claimName) const;
    std::optional<std::string> stringForClaim(const std::string_view& claimName) const;

    void checkJwtFormat() const;
    void checkRequiredClaims() const;
    void checkAudClaim() const;
    void checkIfExpired() const;

    constexpr static std::string_view iatClaim = "iat";
    constexpr static std::string_view expClaim = "exp";
    constexpr static std::string_view nbfClaim = "nbf";
    constexpr static std::string_view issClaim = "iss";
    constexpr static std::string_view subClaim = "sub";
    constexpr static std::string_view nonceClaim = "nonce";
    constexpr static std::string_view acrClaim = "acr";
    constexpr static std::string_view audClaim = "aud";
    constexpr static std::string_view professionOIDClaim = "professionOID";
    constexpr static std::string_view givenNameClaim = "given_name";
    constexpr static std::string_view familyNameClaim = "family_name";
    constexpr static std::string_view organizationNameClaim = "organizationName";
    constexpr static std::string_view idNumberClaim = "idNummer";
    constexpr static std::string_view jtiClaim = "jti";
    constexpr static std::string_view clientIdClaim = "client_id";
    constexpr static std::string_view acrContent = "gematik-ehealth-loa-high";
    constexpr static std::chrono::seconds A20373_iatClaimToleranceSeconds = std::chrono::seconds(2);

private:
    std::optional<std::string> stringForKey(const rapidjson::Document& doc,
                                            const std::string_view& key) const;
    std::optional<int64_t> intForKey(const rapidjson::Document& doc,
                                     const std::string_view& key) const;
    std::optional<double> doubleForKey(const rapidjson::Document& doc,
                                      const std::string_view& key) const;

    std::string mJwt{};
    std::string mHeader{};
    std::string mPayload{};
    std::string mSignature{};
    rapidjson::Document mClaims{};
};


#endif
