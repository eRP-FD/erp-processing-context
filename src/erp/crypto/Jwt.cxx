/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/JwtException.hxx"
#include "erp/util/String.hxx"


#include <algorithm>
#include <boost/range/algorithm/count.hpp>
#include <chrono>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/rapidjson.h>

constexpr char separator = '.';

JWT::JWT(const JWT& other)
    : mJwt(other.mJwt), mHeader(other.mHeader), mPayload(other.mPayload), mSignature(other.mSignature), mClaims()
{
    mClaims.CopyFrom(other.mClaims, mClaims.GetAllocator());
}


JWT::JWT(std::string jwt)
    : mJwt(std::move(jwt))
{
    A_19993.start("Check JWT structure according to gemSpec_IDP_FD chapter 6.");
    A_20362.start("RFC 7519 7.2.1, 7.2.2, 7.2.10 - Check that exactly two periods exist, first part is the header, second part is the claims document.");

    int64_t periods = boost::count(mJwt, '.');
    // RFC 7519 7.2.1
    if (periods != 2)
    {
        Fail2("Pre-verification failed - expecting JWS Compact Serialization.", JwtInvalidFormatException);
    }

    auto parts = String::split(mJwt, ".");
    mHeader = parts[0];
    mPayload = parts[1];
    A_20362.finish();
    mSignature = parts[2];

    const rapidjson::ParseResult result = mClaims.Parse(Base64::decodeToString(mPayload));
    if (result.IsError())
    {
        Fail2("Pre-verification failed - erroneous claims document.", JwtInvalidFormatException);
    }
    A_19993.finish();
}

bool JWT::operator==(const JWT& jwt) const
{
    return (mJwt == jwt.mJwt
            && mHeader == jwt.mHeader
            && mPayload == jwt.mPayload
            && mSignature == jwt.mSignature
            && mClaims == jwt.mClaims);
}

std::string JWT::serialize(void) const
{
    return mHeader + separator + mPayload + separator + mSignature;
}

// GEMREQ-start A_19439#verify
void JWT::verify(const shared_EVP_PKEY& publicKey) const
{
    checkJwtFormat();
    checkRequiredClaims();
    checkAudClaim();
    checkIfExpired();
    verifySignature(publicKey);
}
// GEMREQ-end A_19439#verify

void JWT::checkJwtFormat() const
{
    A_19993.start("Check JWT structure according to gemSpec_IDP_FD chapter 6.");

    A_20362.start("RFC 7519 7.2.2, 7.2.3, 7.2.4, 7.2.5 - Decode b64encoded header, valid UTF-8 verification is handled in rapidjsons Parse call, verify required header fields.");

    // 7.2.2
    if (mHeader.empty())
    {
        Fail2("Pre-verification failed - JWT violates RFC 7519.", JwtInvalidRfcFormatException);
    }

    // 7.2.3, 7.2.4
    rapidjson::Document headerDict{};
    std::string decodedHeader{};
    try
    {
        decodedHeader = Base64::decodeToString(mHeader);
    }
    catch (const std::exception&)
    {
        // Handle invalid Base64 encoded string as JwtInvalidRfcFormatException in order to comply with A_19131.
        Expect3(false, "Pre-verification failed - JWT violates RFC 7519.", JwtInvalidRfcFormatException);
    }
    const rapidjson::ParseResult headerResult = headerDict.Parse(decodedHeader);
    Expect3(!headerResult.IsError(), "Pre-verification failed - JWT violates RFC 7519.", JwtInvalidRfcFormatException);

    // 7.2.5
    if (!headerDict.HasMember("alg"))
    {
        Fail2("Pre-verification failed - Missing signature algorithm name.", JwtInvalidRfcFormatException);
    }
    const std::string alg = headerDict["alg"].GetString();
    if (alg != "BP256R1")
    {
        Fail2("Pre-verification failed - unsupported signature algorithm requested.", JwtInvalidSignatureException);
    }
    A_20362.finish();

    A_20504.start("Check for empty signature.");
    if (mSignature.empty())
    {
        Fail2("Pre-verification failed - missing signature.", JwtInvalidSignatureException);
    }
    A_20504.finish();

    A_19993.finish();
}

void JWT::verifySignature(const shared_EVP_PKEY& publicKey) const
{
    Expect(publicKey != nullptr, "Missing public key");
    Expect(EVP_PKEY_id(publicKey) == EVP_PKEY_EC, "Wrong pubkey information");
    Expect(EVP_PKEY_bits(publicKey) == 256, "Wrong pubkey bit length");

    // Create digest and fill with data.
    auto ctx = shared_EVP_MD_CTX::make();
    Expect(ctx != nullptr, "Can't create context");
    Expect(EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr,
                                publicKey.removeConst()) == 1,
           "Can't create digest structure");
    Expect(EVP_DigestVerifyUpdate(ctx, mHeader.data(), mHeader.size()) == 1,
           "Can't update digest structure with header");
    Expect(EVP_DigestVerifyUpdate(ctx, &separator, 1) == 1,
           "Can't update digest structure with separator");
    Expect(EVP_DigestVerifyUpdate(ctx, mPayload.data(), mPayload.size()) == 1,
           "Can't update digest structure with payload");

    // Create signature from string.
    const int veclen = 32;
    std::array<unsigned char, veclen> rvec{0};
    std::array<unsigned char, veclen> svec{0};
    const auto binarySignature = Base64::decodeToString(mSignature);
    // Two number buffers (r and s values), 2x32 bytes.
    A_20504.start("Check for problematic/corrupted signature.");
    if (binarySignature.size() != 2ul * veclen)
    {
        Fail2("Verification failed - invalid binary signature.", JwtInvalidSignatureException);
    }
    A_20504.finish();
    std::copy_n(std::begin(binarySignature), rvec.size(), std::begin(rvec));
    std::copy_n(std::begin(binarySignature) + rvec.size(), svec.size(), std::begin(svec));

    auto sig = shared_ECDSA_SIG::make();
    Expect(sig != nullptr, "Can't create sig object");
    auto r = shared_BN::make();
    Expect(r != nullptr, "Can't create r object");
    auto s = shared_BN::make();
    Expect(s != nullptr, "Can't create s object");
    Expect(BN_bin2bn(rvec.data(), veclen, r) != nullptr, "Failed call to BN_bin2bn (r)");
    Expect(BN_bin2bn(svec.data(), veclen, s) != nullptr, "Failed call to BN_bin2bn (s)");
    // The call below transfers ownership of r and s to sig, release them to avoid double deallocation.
    Expect(ECDSA_SIG_set0(sig, r.release(), s.release()) == 1, "Can't initialize signature");

    // Convert "jose->DER" format (2x32 bytes + 6 bytes info).
    int siglen = i2d_ECDSA_SIG(sig, nullptr);
    Expect(siglen > 0, "Failed to get signature length.");
    std::vector<unsigned char> sig_der_bytes(static_cast<size_t>(siglen), 0u);
    unsigned char* rawptr = sig_der_bytes.data();
    Expect(i2d_ECDSA_SIG(sig, &rawptr) != 0, "Failed call to i2d_ECDSA_SIG");

    A_20365.start("Verify token signature with the given public key.");
    A_20504.start("Check for valid signature.");
    // Verification
    if (EVP_DigestVerifyFinal(ctx, sig_der_bytes.data(), gsl::narrow<size_t>(siglen)) != 1)
    {
        Fail2("Verification failed - invalid signature or payload.", JwtInvalidSignatureException);
    }
    A_20504.finish();
    A_20365.finish();
}

std::optional<std::string> JWT::stringForKey(const rapidjson::Document& doc,
                                             const std::string_view& key) const
{
    auto keyRef = rapidjson::StringRef(key.data(), key.size());
    if (doc.IsObject() && doc.HasMember(keyRef) && doc[keyRef.s].IsString())
    {
        return doc[keyRef.s].GetString();
    }
    return std::nullopt;
}


std::optional<int64_t> JWT::intForKey(const rapidjson::Document& doc,
                                      const std::string_view& key) const
{
    auto keyRef = rapidjson::StringRef(key.data(), key.size());
    if (doc.IsObject() && doc.HasMember(keyRef) && doc[keyRef.s].IsUint64())
    {
        return doc[keyRef.s].GetInt64();
    }
    return std::nullopt;
}

std::optional<double> JWT::doubleForKey(const rapidjson::Document& doc,
                                        const std::string_view& key) const
{
    auto keyRef = rapidjson::StringRef(key.data(), key.size());
    if (doc.IsObject() && doc.HasMember(keyRef) && doc[keyRef.s].IsDouble())
    {
        return doc[keyRef.s].GetDouble();
    }
    return std::nullopt;
}

std::optional<std::int64_t> JWT::intForClaim(const std::string_view& claimName) const
{
    A_19390.start("Retrieve the professionOID claim from JWT");
    return intForKey(mClaims, claimName);
}


std::optional<double> JWT::doubleForClaim(const std::string_view& claimName) const
{
    return doubleForKey(mClaims, claimName);
}


std::optional<std::string> JWT::stringForClaim(const std::string_view& claimName) const
{
    A_19390.start("Retrieve the professionOID claim from JWT");
    return stringForKey(mClaims, claimName);
}


void JWT::checkRequiredClaims() const
{
    auto checkClaimsPresence = [this] (const std::vector<std::string_view>& RequiredClaims) {
        A_20368.start("Check that required claims are provided.");
        for (const auto& claim : RequiredClaims)
        {
            if (mClaims.HasMember(std::string{claim}) == false)
            {
                Fail2("Pre-verification failed - Missing required claims.", JwtRequiredClaimException);
            }
        }
        A_20368.finish();
    };

    if (mClaims.HasParseError())
    {
        Fail2("Pre-verification failed - Problematic claims document.", JwtInvalidFormatException);
    }

    checkClaimsPresence( { JWT::professionOIDClaim} );

    Expect3(mClaims[std::string{JWT::professionOIDClaim}].IsString(), "Pre-verification failed - invalid data type for professionOID claim.", JwtInvalidFormatException);
    auto professionOid = stringForClaim(std::string{JWT::professionOIDClaim});

    A_20370.start("Check data type for each claim.");
    if (professionOid == profession_oid::oid_versicherter)
    {
        checkClaimsPresence( { JWT::iatClaim,
                JWT::expClaim,
                JWT::issClaim,
                JWT::subClaim,
                JWT::acrClaim,
                JWT::audClaim,
                JWT::givenNameClaim,
                JWT::familyNameClaim,
                JWT::organizationNameClaim,
                JWT::idNumberClaim,
                JWT::jtiClaim } );

        if (! (mClaims[std::string{JWT::iatClaim}].IsInt64() &&
               mClaims[std::string{JWT::expClaim}].IsInt64() &&
               mClaims[std::string{JWT::issClaim}].IsString() &&
               mClaims[std::string{JWT::subClaim}].IsString() &&
               mClaims[std::string{JWT::acrClaim}].IsString() &&
               mClaims[std::string{JWT::audClaim}].IsString() &&
               mClaims[std::string{JWT::givenNameClaim}].IsString() &&
               mClaims[std::string{JWT::familyNameClaim}].IsString() &&
               mClaims[std::string{JWT::organizationNameClaim}].IsString() &&
               mClaims[std::string{JWT::idNumberClaim}].IsString() &&
               mClaims[std::string{JWT::jtiClaim}].IsString()))
        {
            Fail2("Pre-verification failed - invalid data type for claims.", JwtInvalidFormatException);
        }
    }
    else
    {
        checkClaimsPresence( { JWT::iatClaim,
                JWT::expClaim,
                JWT::issClaim,
                JWT::subClaim,
                JWT::acrClaim,
                JWT::audClaim,
                JWT::idNumberClaim,
                JWT::jtiClaim } );

        if (! (mClaims[std::string{JWT::iatClaim}].IsInt64() &&
               mClaims[std::string{JWT::expClaim}].IsInt64() &&
               mClaims[std::string{JWT::issClaim}].IsString() &&
               mClaims[std::string{JWT::subClaim}].IsString() &&
               mClaims[std::string{JWT::acrClaim}].IsString() &&
               mClaims[std::string{JWT::audClaim}].IsString() &&
               mClaims[std::string{JWT::idNumberClaim}].IsString() &&
               mClaims[std::string{JWT::jtiClaim}].IsString()))
        {
            Fail2("Pre-verification failed - invalid data type for claims.", JwtInvalidFormatException);
        }
    }
    A_20370.finish();

    // GEMREQ-start A_19439#claims
    A_19439.start("Check for a specific authentication strength claim.");
    auto acr = stringForClaim(JWT::acrClaim);
    Expect(acr.has_value(), "Missing required acr claim.");
    if (acr != acrContent)
    {
        Fail2("The provided acr claim is not supported.", JwtInvalidFormatException);
    }
    A_19439.finish();
    // GEMREQ-end A_19439#claims
}

void JWT::checkAudClaim() const
{
    A_21520.start("Check tokens aud claim");
    Expect3(stringForClaim(JWT::audClaim).value() == Configuration::instance().getStringValue(ConfigurationKey::IDP_REGISTERED_FD_URI),
            "The provided aud claim does not match. " + stringForClaim(JWT::audClaim).value(),
            JwtInvalidAudClaimException);
    A_21520.finish();
}

void JWT::checkIfExpired() const
{
    /**
     *    ----+--------------+------+---------+--------+-------------->
     *      [iat            nbf*   tNow    iat+300)   exp)      t [s]
     *
     *    where:
     *           iat        <= tNow + 2s
     *           when nbf* > 0: tNow in [nbf ; exp)
     *                nbf* = 0: tNow in [iat ; exp)
     */
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());

    // In any case and for whatever reason, if no iat or exp is passed,
    // we consider the given token as expired.
    const auto iat = std::chrono::seconds(intForKey(mClaims, iatClaim).value_or(0));
    const auto exp = std::chrono::seconds(intForKey(mClaims, expClaim).value_or(0));
    const auto nbf = std::chrono::seconds(intForKey(mClaims, nbfClaim).value_or(0));

    if (nbf.count() > 0)
    {
        A_20374.start("Use nbf instead of iat when provided.");
        A_20374.finish();
    } else {
        A_20373.start("When no nbf is provided, continue with A_20372 and A_19902.");
        A_20373.finish();
    }

    A_20374.start("Use nbf instead of iat when provided.");
    A_20373.start("Allow 2 second tolerance when checking iat claim.");
    A_20372.start("Check if the JWT is not expired (time is between iat and exp).");
    A_19902.start("Check if the issued JWT is expired.");
    if (now > exp)
    {
        // Expired
        Fail2("Verification failed - Token expired now=" + std::to_string(now.count()) +
                                  " exp=" + std::to_string(exp.count()), JwtExpiredException);
    }
    else if (iat > (now + A20373_iatClaimToleranceSeconds))
    {
        // Issued for a later time than current time.
        Fail2("Verification failed - Token expired (issued for a later time).", JwtExpiredException);
    }
    else if (now < nbf)
    {
        // Access time violates not-before timestamp.
        Fail2("Verification failed - Token nbf violated.", JwtExpiredException);
    }
    A_20372.finish();
    A_20373.finish();
    A_20374.finish();
    A_19902.finish();
}
