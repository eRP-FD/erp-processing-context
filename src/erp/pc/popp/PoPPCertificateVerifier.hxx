/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/crypto/Jwt.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/util/PeriodicTimer.hxx"

#include <shared_mutex>


namespace model
{
struct PoPPCertificateHealthData;
}
class TslManager;


class PoPPCertificateVerifier : public std::enable_shared_from_this<PoPPCertificateVerifier>, public TimerHandler
{
public:
    // --------------------------------------------------------------------------------

    class JwtDownload
    {
    public:
        explicit JwtDownload(const std::shared_ptr<UrlRequestSender>& sender);

        /**
         * @brief Downloads a JWT from the given URL.
         *
         * Performs an HTTP GET request to `downloadUrl` and attempts to parse
         * the response body as a JWT.
         *
         * On failure (HTTP error, connection timeout, invalid JWT format), logs
         * a warning and returns std::nullopt, allowing the caller to retain the
         * existing cached state.
         *
         * @param downloadUrl The URL to download the JWT from.
         *
         * @return The parsed JWT if the download and parsing succeeded,
         *         std::nullopt otherwise.
         */
        std::optional<JWT> download(const std::string& downloadUrl);

    private:
        std::shared_ptr<UrlRequestSender> mRequestSender;
    };

    class EcJwk
    {
    public:
        EcJwk(std::string kty, std::string kid, std::string crv, std::string x, std::string y, std::string x5tS256,
              std::vector<std::string> x5c)
            : mKty(std::move(kty))
            , mKid(std::move(kid))
            , mCrv(std::move(crv))
            , mX(std::move(x))
            , mY(std::move(y))
            , mX5tS256(std::move(x5tS256))
            , mX5c(std::move(x5c))
        {
        }

        const std::string& kty() const noexcept
        {
            return mKty;
        }
        const std::string& kid() const noexcept
        {
            return mKid;
        }
        const std::string& crv() const noexcept
        {
            return mCrv;
        }
        const std::string& x() const noexcept
        {
            return mX;
        }
        const std::string& y() const noexcept
        {
            return mY;
        }
        const std::string& x5tS256() const noexcept
        {
            return mX5tS256;
        }
        const std::vector<std::string>& x5c() const noexcept
        {
            return mX5c;
        }

        [[nodiscard]] shared_EVP_PKEY calculatePublicKey() const;

    private:
        std::string mKty;
        std::string mKid;
        std::string mCrv;
        std::string mX;
        std::string mY;
        std::string mX5tS256;
        std::vector<std::string> mX5c;
    };

    // --------------------------------------------------------------------------------

    class JwksDocument
    {
    public:
        explicit JwksDocument(const JWT& jwt);
        virtual ~JwksDocument() = default;
        const std::vector<EcJwk>& getKeys() const noexcept;
        std::string getHeaderValue(const std::string& key);

        static EcJwk parseKey(const rapidjson::Value& key);

        void validateHeader() const;
        void verifySignature(const shared_EVP_PKEY& publicKey) const;

    protected:
        void parseKeys(rapidjson::Value::ConstMemberIterator& keysIt);

        JWT mJwt;
        std::vector<EcJwk> mKeys;
    };

    // --------------------------------------------------------------------------------

    class EntityStatement : public JwksDocument
    {
    public:
        explicit EntityStatement(const JWT& jwt);

        const EcJwk& validateKeys() const;

        [[nodiscard]] const shared_EVP_PKEY& publicKey() const;

        /**
         * @brief Extracts the JWK-Set URI from a PoPP Service Entity Statement JWT.
         *
         * Parses the claims of `jwt` and retrieves the value of
         * metadata.oauth_resource.signed_jwks_uri (A_26449).
         *
         * @param jwt The parsed Entity Statement JWT.
         * @return The JWK-Set URI extracted from the Entity Statement.
         * @throws model::ModelException if the JWT claims are missing, malformed,
         *         or the signed_jwks_uri claim is absent.
         */
        [[nodiscard]] std::string getJwksUri() const;
        [[nodiscard]] std::string_view sub() const;

        static shared_EVP_PKEY calculatePublicKey(const EcJwk& jwk);
    };

    // --------------------------------------------------------------------------------

    class Jwks : public JwksDocument
    {
    public:
        explicit Jwks(const JWT& jwt);
    };

    // --------------------------------------------------------------------------------

    class PoPPCertificateCache
    {
    public:
        struct Key {
            std::string kid;
            std::optional<model::Timestamp> lastSuccessAt{};
            model::Timestamp expiration{model::Timestamp::now()};
            std::shared_ptr<X509Certificate> certificate;
            shared_EVP_PKEY publicKey;
            bool lastCheckExpired(const std::chrono::steady_clock::duration& maxAge) const;
        };

        void invalidateAndUpdate(const PoPPCertificateVerifier::Jwks& jwks);
        std::optional<Key> getKey(const std::string& kid) const;
        std::list<Key> getKeys() const;

        template<typename Predicate>
        void removeIf(Predicate&& predicate)
        {
            std::unique_lock lock(mCacheMutex);
            std::erase_if(mCache, [&predicate](auto& pair) {
                return std::forward<Predicate>(predicate)(pair.second);
            });
        }

        std::map<std::string, Key> mCache{};
        mutable std::shared_mutex mCacheMutex;
    };

    // --------------------------------------------------------------------------------

    /**
     * @param context  The io_context used to schedule the timer.
     */
    explicit PoPPCertificateVerifier(boost::asio::io_context& context, TslManager& mTslManager,
                                     std::shared_ptr<CrlProvider> crlProvider);
    PoPPCertificateVerifier(boost::asio::io_context& context, std::unique_ptr<UrlRequestSender> urlRequestSender,
                            TslManager& mTslManager);

    void setup(std::chrono::steady_clock::duration verifyInterval, std::chrono::steady_clock::duration maxAgeSeconds,
               int connectTimeoutSeconds, int responseTimeoutSeconds,
               std::chrono::steady_clock::duration errorCaseInterval);

    boost::asio::io_context& context() const;

    std::optional<EntityStatement> getEntityStatement() const;

    std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> getKey(const std::string& kid) const;
    std::list<model::PoPPCertificateHealthData> getHealthData() const;

private:
    /**
     * @brief Downloads and parses the PoPP Service Entity Statement.
     *
     * Fetches the Entity Statement JWT from the URL configured via
     * POPP_ENTITY_STATEMENT_URL and extracts the JWK-Set URI from
     * the claim metadata.oauth_resource.signed_jwks_uri (A_26449).
     *
     * On failure (HTTP error, invalid JWT, parse error), logs a warning
     * and leaves the existing cache unchanged.
     *
     * @return The JWK-Set URI (signed_jwks_uri) if the fetch and parse
     *         succeeded, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<JWT> fetchEntityStatement();

    /**
     * @brief Downloads and parses the PoPP Service JWK-Set from the given URL.
     *
     * Fetches the signed JWK-Set JWT, [wip: verifies its signature
     * using the Entity Statement signing key (A_26534), and updates the local
     * certificate cache with the contained public keys (A_26449).]
     *
     * On failure (HTTP error, invalid JWT, parse error), logs a warning
     * and leaves the existing cache unchanged.
     *
     * @param downloadUrl The URL obtained from the Entity Statement
     *
     * @return true if the fetch, signature verification and cache update
     *         succeeded, false otherwise.
     */
    [[nodiscard]] std::optional<JWT> fetchJwkSet(const std::string& downloadUrl);

    void performDownloadAndVerification();

    void verifyJwksDocuments();
    void verifyCachedCertificates();

    void timerHandler() override;
    std::optional<std::chrono::steady_clock::duration> nextInterval() const override;

    int mConnectTimeoutSeconds{5};
    int mResponseTimeoutSeconds{30};
    std::chrono::steady_clock::duration mVerifyInterval{std::chrono::seconds(3600)};
    std::chrono::steady_clock::duration mMaxAgeSeconds{std::chrono::hours(24)};
    boost::asio::io_context& mContext;
    std::shared_ptr<CrlProvider> mCrlProvider;
    std::shared_ptr<UrlRequestSender> mRequestSender;
    std::unique_ptr<JwtDownload> mDownloader;
    std::optional<EntityStatement> mEntityStatement;
    std::optional<Jwks> mJwks;
    std::chrono::steady_clock::duration mErrorCaseNextInterval{std::chrono::seconds(30)};
    PoPPCertificateCache mCache;
    TslManager& mTslManager;
};
