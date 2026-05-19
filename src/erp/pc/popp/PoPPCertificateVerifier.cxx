/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPCertificateVerifier.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/model/Health.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/JwtException.hxx"

#include <chrono>
#include <vector>


namespace
{

std::string readCa()
{
    std::string rootCaStr;
    const std::string rootCaPath =
        Configuration::instance().getStringValue(ConfigurationKey::IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH);
    if (! rootCaPath.empty())
    {
        rootCaStr = FileHelper::readFileAsString(rootCaPath);
    }
    else
    {
        TVLOG(1) << "No IDP update endpoint SSL root CA configured";
    }
    return rootCaStr;
}

}

shared_EVP_PKEY PoPPCertificateVerifier::EcJwk::calculatePublicKey() const
{
    return EllipticCurveUtils::createPublicKeyBin(Base64::decodeToString(x()), Base64::decodeToString(y()),
                                                  NID_X9_62_prime256v1);
}

PoPPCertificateVerifier::EcJwk PoPPCertificateVerifier::JwksDocument::parseKey(const rapidjson::Value& key)
{
    ModelExpect(key.IsObject(), "key is not an object.");

    auto ktyIt = key.FindMember("kty");
    ModelExpect(ktyIt != key.MemberEnd() && ktyIt->value.IsString(), "Missing or invalid kty.");

    const std::string kty = ktyIt->value.GetString();
    ModelExpect(kty == "EC", "Unsupported kty.");

    auto kidIt = key.FindMember("kid");
    ModelExpect(kidIt != key.MemberEnd() && kidIt->value.IsString(), "Missing or invalid kid.");
    const std::string kid = kidIt->value.GetString();

    auto crvIt = key.FindMember("crv");
    ModelExpect(crvIt != key.MemberEnd() && crvIt->value.IsString(), "Missing or invalid crv.");

    const std::string crv = crvIt->value.GetString();
    ModelExpect(crv == "P-256", "Unsupported crv.");

    auto xIt = key.FindMember("x");
    ModelExpect(xIt != key.MemberEnd() && xIt->value.IsString(), "Missing or invalid x coord.");
    const std::string x = xIt->value.GetString();

    auto yIt = key.FindMember("y");
    ModelExpect(yIt != key.MemberEnd() && yIt->value.IsString(), "Missing or invalid y coord.");
    const std::string y = yIt->value.GetString();

    if (key.HasMember("x5c"))
    {
        auto x5cIt = key.FindMember("x5c");
        ModelExpect(x5cIt != key.MemberEnd(), "Missing x5c array.");
        const auto arr = x5cIt->value.GetArray();
        ModelExpect(arr.Size() > 0, "Empty x5c array.");
        if (arr.Size() > 1)
        {
            TLOG(INFO) << "Multiple certificates for kid, picking first.";
        }

        return {kty, kid, crv, x, y, "", {arr[0].GetString()}};
    }
    return {kty, kid, crv, x, y, "", {""}};
}

void PoPPCertificateVerifier::JwksDocument::verifySignature(const shared_EVP_PKEY& publicKey) const
{
    mJwt.verifySignature(publicKey);
}

PoPPCertificateVerifier::JwksDocument::JwksDocument(const JWT& jwt)
    : mJwt(jwt)
{
}

void PoPPCertificateVerifier::JwksDocument::parseKeys(rapidjson::Value::ConstMemberIterator& keysIt)
{
    const rapidjson::Value& keysArray = keysIt->value;
    for (const auto& key : keysArray.GetArray())
    {
        ModelExpect(key.IsObject(), "Unexpected object in keys.");
        mKeys.push_back(parseKey(key));
    }
    ModelExpect(not mKeys.empty(), "No keys found.");
}

const std::vector<PoPPCertificateVerifier::EcJwk>& PoPPCertificateVerifier::JwksDocument::getKeys() const noexcept
{
    return mKeys;
}

std::string PoPPCertificateVerifier::JwksDocument::getHeaderValue(const std::string& key)
{
    const auto header = mJwt.headerDocument();
    const auto valueIt = header.FindMember(key);
    ModelExpect(valueIt != header.MemberEnd() && valueIt->value.IsString(), "Missing or invalid header field: " + key);
    return valueIt->value.GetString();
}

void PoPPCertificateVerifier::JwksDocument::validateHeader() const
{
    const auto header = mJwt.headerDocument();
    // alg property is checked in the Jwt class during creation.
    ModelExpect(header["alg"].GetString() == std::string{"ES256"}, "Unexpected alg in jwks JWT.");
    const auto kidIt = header.FindMember("kid");
    ModelExpect(kidIt != header.MemberEnd() && kidIt->value.IsString(), "Missing or invalid kid.");
}

PoPPCertificateVerifier::EntityStatement::EntityStatement(const JWT& jwt)
    : JwksDocument(jwt)
{
    // Instances of the Jwt class always have a valid claims document.
    const auto& payload = jwt.claimsDocument();

    const auto jwksIt = payload.FindMember("jwks");
    ModelExpect(jwksIt != payload.MemberEnd() && jwksIt->value.IsObject(), "Invalid jwks.");

    rapidjson::Value::ConstMemberIterator keysIt = jwksIt->value.FindMember("keys");
    ModelExpect(keysIt != jwksIt->value.MemberEnd() && keysIt->value.IsArray(), "Invalid keys.");

    parseKeys(keysIt);
}

const PoPPCertificateVerifier::EcJwk& PoPPCertificateVerifier::EntityStatement::validateKeys() const
{
    const auto header = mJwt.headerDocument();

    const auto kidIt = header.FindMember("kid");
    ModelExpect(kidIt != header.MemberEnd() && kidIt->value.IsString(), "Missing or invalid kid.");
    const std::string kid = kidIt->value.GetString();
    ModelExpect(not kid.empty(), "Missing kid in jwks JWT.");

    const auto& keys = getKeys();

    const auto it = std::ranges::find_if(keys, [&kid](const EcJwk& key) {
        return key.kid() == kid;
    });
    ModelExpect(it != keys.end(), "No matching kid found in JWKS");

    return *it;
}

shared_EVP_PKEY PoPPCertificateVerifier::EntityStatement::calculatePublicKey(const EcJwk& jwk)
{
    return EllipticCurveUtils::createPublicKeyBin(Base64::decodeToString(jwk.x()), Base64::decodeToString(jwk.y()),
                                                  NID_X9_62_prime256v1);
}

std::string PoPPCertificateVerifier::EntityStatement::getJwksUri() const
{
    // All jwt instances have a claims document.
    const auto& doc = mJwt.claimsDocument();
    ModelExpect(doc.HasMember("metadata"), "No metadata in claims found.");
    const auto& metadata = doc["metadata"];

    ModelExpect(metadata.IsObject() && metadata.HasMember("oauth_resource"), "Missing resource in metadata found.");
    const auto& res = metadata["oauth_resource"];
    ModelExpect(res.IsObject(), "Unsupported resource in metadata found.");

    // GEMREQ-start A_26449#extract-signed_jwks_uri
    auto it = res.FindMember("signed_jwks_uri");
    ModelExpect(it != res.MemberEnd() && it->value.IsString(), "Missing jwks_uri.");
    return it->value.GetString();
    // GEMREQ-end A_26449#extract-signed_jwks_uri
}

std::string_view PoPPCertificateVerifier::EntityStatement::sub() const
{
    const auto& doc = mJwt.claimsDocument();
    ModelExpect(doc.IsObject() && doc.HasMember("sub"), "No sub in claims found.");
    return doc["sub"].GetString();
}


PoPPCertificateVerifier::Jwks::Jwks(const JWT& jwt)
    : JwksDocument(jwt)
{
    // Instances of the Jwt class always have a valid claims document.
    const auto& payload = jwt.claimsDocument();

    rapidjson::Value::ConstMemberIterator keysIt = payload.FindMember("keys");
    ModelExpect(keysIt != payload.MemberEnd() && keysIt->value.IsArray(), "Invalid keys.");

    parseKeys(keysIt);
}


PoPPCertificateVerifier::JwtDownload::JwtDownload(const std::shared_ptr<UrlRequestSender>& sender)
    : mRequestSender(sender)
{
}


std::optional<JWT> PoPPCertificateVerifier::JwtDownload::download(const std::string& downloadUrl)
{
    try
    {
        TVLOG(1) << "Download JWT from " << downloadUrl;
        const auto response = mRequestSender->send(downloadUrl, HttpMethod::GET, "", {}, std::nullopt, false);
        if (response.getHeader().status() == HttpStatus::OK)
        {
            return JWT(response.getBody());
        }
        TLOG(WARNING) << "Unexpected HTTP status: " << response.getHeader().status() << ", from " << downloadUrl;
    }
    catch (const JwtInvalidFormatException& exc)
    {
        TLOG(WARNING) << "Downloaded JWT with error: " << exc.what();
    }
    catch (const model::ModelException& exc)
    {
        TLOG(WARNING) << "Failed downloading JWT, model: " << exc.what();
    }
    catch (const std::runtime_error& exc)
    {
        TLOG(WARNING) << "Failed downloading JWT, runtime error: " << exc.what();
    }
    return std::nullopt;
}


bool PoPPCertificateVerifier::PoPPCertificateCache::Key::lastCheckExpired(
    const std::chrono::steady_clock::duration& maxAge) const
{
    if (! lastSuccessAt.has_value())
    {
        return true;
    }

    const auto age = model::Timestamp::now() - *lastSuccessAt;
    if (age < std::chrono::steady_clock::duration::zero())
    {
        TLOG(WARNING) << "lastSuccessAt is in the future, treating as expired.";
        return true;
    }
    return age > maxAge;
}


void PoPPCertificateVerifier::PoPPCertificateCache::invalidateAndUpdate(const PoPPCertificateVerifier::Jwks& jwks)
{
    const auto& keys = jwks.getKeys();

    if (keys.empty())
    {
        TVLOG(1) << "No keys in jwks.";
        return;
    }

    {
        std::unique_lock lock(mCacheMutex);
        mCache.clear();
        for (const auto& k : keys)
        {
            if (not mCache.contains(k.kid()) && not k.x5c().empty())
            {
                const auto x5c = k.x5c().at(0);
                const auto cert = std::make_shared<X509Certificate>(X509Certificate::createFromBase64(x5c));
                const auto calculatedPublicKey = k.calculatePublicKey();
                auto result = Key{
                    .kid = k.kid(),
                    .lastSuccessAt = std::nullopt, // timestamp will be set after successful TSL check.
                    .expiration = model::Timestamp::fromTmInUtc(cert->getNotAfter()),
                    .certificate = cert,
                    .publicKey = calculatedPublicKey};
                mCache[k.kid()] = result;
            }
        }
    }
}


std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key>
PoPPCertificateVerifier::PoPPCertificateCache::getKey(const std::string& kid) const
{
    std::shared_lock lock(mCacheMutex);
    auto it = mCache.find(kid);
    if (it != mCache.end())
    {
        return it->second;
    }
    return std::nullopt;
}

std::list<PoPPCertificateVerifier::PoPPCertificateCache::Key> PoPPCertificateVerifier::PoPPCertificateCache::getKeys() const
{
    std::shared_lock lock(mCacheMutex);
    std::list<Key> keys;
    for (const auto& key : mCache | std::views::values)
    {
        keys.emplace_back(key);
    }
    return keys;
}


PoPPCertificateVerifier::PoPPCertificateVerifier(boost::asio::io_context& context, TslManager& tslManager,
                                                 std::shared_ptr<CrlProvider> crlProvider)
    : mContext(context)
    , mCrlProvider(std::move(crlProvider))
    , mRequestSender(std::make_unique<UrlRequestSender>(
          TlsCertificateVerifier::withCustomRootCertificates(readCa()).withCrl(
              *mCrlProvider, TlsCertificateVerifier::CrlMode::SOFT_FAIL),
          std::chrono::seconds{
              Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
          std::chrono::milliseconds{
              Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)}))
    , mDownloader(std::make_unique<JwtDownload>(mRequestSender))
    , mTslManager(tslManager)
{
    mRequestSender->setProxies(Configuration::instance().proxyParameters(ProxyMode::SNI));
}

PoPPCertificateVerifier::PoPPCertificateVerifier(boost::asio::io_context& context,
                                                 std::unique_ptr<UrlRequestSender> urlRequestSender,
                                                 TslManager& tslManager)
    : mContext(context)
    , mRequestSender(std::move(urlRequestSender))
    , mDownloader(std::make_unique<JwtDownload>(mRequestSender))
    , mTslManager(tslManager)
{
}


void PoPPCertificateVerifier::setup(std::chrono::steady_clock::duration verifyInterval,
                                    std::chrono::steady_clock::duration maxAgeSeconds, int connectTimeoutSeconds,
                                    int responseTimeoutSeconds, std::chrono::steady_clock::duration errorCaseInterval)
{
    mConnectTimeoutSeconds = connectTimeoutSeconds;
    mResponseTimeoutSeconds = responseTimeoutSeconds;
    mVerifyInterval = verifyInterval;
    mMaxAgeSeconds = maxAgeSeconds;
    mErrorCaseNextInterval = errorCaseInterval;
}

void PoPPCertificateVerifier::performDownloadAndVerification()
{
    TVLOG(1) << "PoPPCertificateVerifier::performDownloadAndVerification call";
    // GEMREQ-start A_26449#download_es
    auto esJwt = fetchEntityStatement();
    // GEMREQ-end A_26449#download_es
    if (esJwt.has_value())
    {
        mEntityStatement.emplace(EntityStatement{*esJwt});
    }
    else
    {
        TLOG(ERROR) << "Could not update the Entity Statement.";
    }

    // Every hour (configurable via POPP_UPDATE_INTERVAL_SECONDS)
    ModelExpect(mEntityStatement, "No Entity Statement available, skipping JWK-Set fetch.");

    // GEMREQ-start A_26534#jwks_sig_check
    // GEMREQ-start A_26449#download_jwks
    auto jwksJwt = fetchJwkSet(mEntityStatement->getJwksUri());
    // GEMREQ-end A_26449#download_jwks
    if (jwksJwt.has_value())
    {
        mJwks.emplace(Jwks{*jwksJwt});
        verifyJwksDocuments();
        mCache.invalidateAndUpdate(*mJwks);
    }
    else
    {
        TLOG(WARNING) << "Could not download JWK set.";
    }
    // GEMREQ-end A_26534#jwks_sig_check

    // GEMREQ-start A_27015#always_check
    verifyCachedCertificates();
    // GEMREQ-end A_27015#always_check
}

void PoPPCertificateVerifier::verifyCachedCertificates()
{
    TVLOG(1) << "PoPPCertificateVerifier::verifyCachedCertificates call";

    // GEMREQ-start A_27015#cert_check
    mCache.removeIf([this](PoPPCertificateCache::Key& key) -> bool {
        try
        {
            // GEMREQ-start A_27016#check_details
            const OcspCheckDescriptor descriptor{
                .mode = OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,

                .timeSettings = {.referenceTimePoint = std::nullopt,
                                 .gracePeriod = OcspHelper::getOcspGracePeriod(TslMode::TSL)},

                .providedOcspResponse = {}};
            mTslManager.verifyCertificate(TslMode::TSL, *key.certificate, {CertificateType::C_ZD_SIG}, descriptor);
            // GEMREQ-start A_28731#admission
            key.certificate->checkRoles({std::string{profession_oid::oid_popp_token}});
            // GEMREQ-end A_28731#admission
            key.lastSuccessAt = model::Timestamp::now();
            // GEMREQ-end A_27016#check_details
            // Keep the key in the cache.
            TLOG(INFO) << "TSL verification success for certificate with kid:" << key.kid
                       << ", subject: " << key.certificate->getSubject()
                       << ", last check:" << (key.lastSuccessAt.has_value() ? key.lastSuccessAt->toXsDateTime() : "never");
            return false;
        }
        catch (const TslError& exc)
        {
            bool removeCertificate = false;
            bool revoked = false;
            const auto& errors = exc.getErrorData();
            for (const TslError::ErrorData& errorData : errors)
            {
                if (errorData.errorCode == TslErrorCode::CERT_REVOKED ||
                    errorData.errorCode == TslErrorCode::CERT_UNKNOWN)
                {
                    revoked = true;
                    removeCertificate = true;
                    break;
                }
            }

            if (revoked)
            {
                TLOG(ERROR) << "Revoked or unknown certificate with kid: " << key.kid
                            << ", subject: " << key.certificate->getSubject()
                            << ", removeCertificate: " << removeCertificate;
            }
            else if ((model::Timestamp::now() - *(key.lastSuccessAt)) > mMaxAgeSeconds)
            {
                TLOG(ERROR) << "Last TSL check too old for certificate with kid: " << key.kid
                            << ", subject: " << key.certificate->getSubject()
                            << ", removeCertificate: " << removeCertificate;
            }
            else
            {
                std::string timeStr{"never"};
                if (key.lastSuccessAt.has_value())
                {
                    const auto minutes =
                        std::chrono::duration_cast<std::chrono::minutes>(model::Timestamp::now() - *(key.lastSuccessAt));
                    timeStr = std::to_string(minutes.count());
                }
                TLOG(WARNING) << "TSL verification failure for certificate with kid: " << key.kid
                              << ", subject: " << key.certificate->getSubject() << ", last check [minutes]: " << timeStr
                              << ", reason: " << exc.what() << ", removeCertificate: " << removeCertificate;
            }

            return removeCertificate;
        }
    });
    // GEMREQ-end A_27015#cert_check
}

void PoPPCertificateVerifier::verifyJwksDocuments()
{
    // GEMREQ-start A_26534#jwks_es_pub_key_check
    ModelExpect(mJwks, "Missing jwks");
    auto kid = mJwks->getHeaderValue("kid");
    const auto allKeys = mEntityStatement->getKeys();
    shared_EVP_PKEY publicKey{};
    for (const auto& key : allKeys)
    {
        if (key.kid() == kid)
        {
            publicKey = EntityStatement::calculatePublicKey(key);
            break;
        }
    }

    if (publicKey)
    {
        mJwks->verifySignature(publicKey);
    }
    else
    {
        TLOG(ERROR) << "Could not find public key for signature check.";
    }
    // GEMREQ-end A_26534#jwks_es_pub_key_check
}

[[nodiscard]] std::optional<JWT> PoPPCertificateVerifier::fetchEntityStatement()
{
    TVLOG(1) << "PoPPCertificateVerifier::fetchEntityStatement call";
    static const auto downloadUrl =
        Configuration::instance().getStringValue(ConfigurationKey::POPP_ENTITY_STATEMENT_URL);
    return mDownloader->download(downloadUrl);
}

[[nodiscard]] std::optional<JWT> PoPPCertificateVerifier::fetchJwkSet(const std::string& downloadUrl)
{
    TVLOG(1) << "PoPPCertificateVerifier::fetchJwkSet";
    return mDownloader->download(downloadUrl);
}

void PoPPCertificateVerifier::timerHandler()
{
    try
    {
        performDownloadAndVerification();
    }
    catch (const std::runtime_error& exc)
    {
        TLOG(WARNING) << exc.what();
    }
}

std::optional<std::chrono::steady_clock::duration> PoPPCertificateVerifier::nextInterval() const
{
    if (mEntityStatement)
    {
        TVLOG(1) << "Received Entity Statement, next check in "
                 << std::chrono::duration_cast<std::chrono::milliseconds>(mVerifyInterval);
        return mVerifyInterval;
    }
    TVLOG(1) << "No Entity Statement, next check in "
             << std::chrono::duration_cast<std::chrono::milliseconds>(mErrorCaseNextInterval);
    return mErrorCaseNextInterval;
}

boost::asio::io_context& PoPPCertificateVerifier::context() const
{
    return mContext;
}

std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key>
PoPPCertificateVerifier::getKey(const std::string& kid) const
{
    std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> result = mCache.getKey(kid);
    if (not result.has_value())
    {
        TLOG(INFO) << "No key with kid: " << kid << " found in cache.";
        return {};
    }

    if (result->lastCheckExpired(mMaxAgeSeconds))
    {
        TLOG(INFO) << "Last check for key with kid: " << kid << " expired.";
        return {};
    }
    return result;
}

std::list<model::PoPPCertificateHealthData> PoPPCertificateVerifier::getHealthData() const
{
    std::list<model::PoPPCertificateHealthData> healthData;
    for (const auto& key : mCache.getKeys())
    {
        healthData.emplace_back(
            key.certificate->getSubject(), key.lastSuccessAt,
            key.lastCheckExpired(mMaxAgeSeconds),
            key.expiration);
    }
    return healthData;
}

std::optional<PoPPCertificateVerifier::EntityStatement> PoPPCertificateVerifier::getEntityStatement() const
{
    return mEntityStatement;
}
