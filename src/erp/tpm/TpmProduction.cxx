/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tpm/TpmProduction.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Hash.hxx"

#include <tpmclient/Client.h>
#include <tpmclient/Session.h>
#include <iostream>


namespace
{
    // Keep the log level high for a couple of days and possible problems with the TPM simulator have been identified
    // and fixed.
    constexpr size_t logLevel = 1;

    std::optional<tpmclient::KeyPairBlob> AttestationKeyPairBlob;

    util::Buffer guardedBase64Decode (const std::string& encoded)
    {
        try
        {
            return Base64::decode(encoded);
        }
        catch(std::exception& e)
        {
            ErpFail(HttpStatus::BadRequest, "invalid base64 string");
        }
    }

    #define catchLogAndRethrow(message)                                                       \
        catch(const std::exception& e)                                                        \
        {                                                                                     \
            TLOG(ERROR) << "TPM: caught exception " << e.what() << " while " << (message);    \
            throw;                                                                            \
        }                                                                                     \
        catch(...)                                                                            \
        {                                                                                     \
            TLOG(ERROR) << "TPM: caught exception while " << (message);                       \
            throw;                                                                            \
        }

    /**
     * LockedTpmClient wraps tpmclient::Client and locks the static mutex on instantiation and unlocks on deletion.
     * Therefore instances of LockedTpmClient are expected to be short lived and only be held inside a method
     * of TpmProduction.
     */
    class LockedTpmClient : public tpmclient::Client
    {
    public:
        LockedTpmClient (const std::shared_ptr<tpmclient::Session>& session, const int32_t logLevel)
            : tpmclient::Client(session),
              mInstanceMutexLock(mTpmProductionMutex),
              mSession(session),
              mLogLevel(logLevel)
        {
            if (mLogLevel >= 0)
                TVLOG(mLogLevel) << "creating TPM for production use";

            mSession->open();

            Expect(isValid(), "TPM client is not valid");
            std::cout << std::flush;
            if (mLogLevel >= 0)
                TVLOG(mLogLevel) << "connection to TPM established and reported as valid";
        }
        ~LockedTpmClient (void)
        {
            if (mLogLevel >= 0)
                TVLOG(mLogLevel) << "releasing connection to TPM";
            mSession->close();
        }

    private:
        /*
         * The TpmProduction is a kind of singleton in that there must not be more than one instance, due to unclear
         * thread safety in the TPM client and session. But in contrast to a singleton, instances will be short lived and
         * at most times there will be no instance at all.
         *
         * This is enforces with the help of a static mutex which is locked while an instance is in use.
         */
        static std::mutex mTpmProductionMutex;
        std::lock_guard<std::mutex> mInstanceMutexLock;
        std::shared_ptr<tpmclient::Session> mSession;
        const int32_t mLogLevel;
    };
    std::mutex LockedTpmClient::mTpmProductionMutex;

    std::unique_ptr<LockedTpmClient> createTpmClient (const int32_t logLevel)
    {
        auto client = std::make_unique<LockedTpmClient>(std::make_shared<tpmclient::Session>(), logLevel);

        return client;
    }
}


std::unique_ptr<Tpm> TpmProduction::createInstance (BlobCache& blobCache, const int32_t loglevel)
{
    return std::make_unique<TpmProduction>(blobCache, loglevel);
}


Tpm::Factory TpmProduction::createFactory (void)
{
    return []
    (BlobCache& blobCache)
    {
        return createInstance(blobCache);
    };
}



TpmProduction::TpmProduction (
    BlobCache& blobCache,
    const int32_t loglevel)
    : mBlobCache(blobCache),
      mLogLevel(loglevel)
{
}


/**
 * A hardware TPM and a simulated TPM are both based on a keypair for the attestation key. This key pair has to be
 * persisted, or otherwise the TPM uses a different attestation key on every restart.
 * This persisting falls to the processing context.
 *
 * This function retrieves the keypair blob from the blob cache. If it is not present, it creates a new one
 * and persists it.
 */
std::vector<uint8_t> TpmProduction::provideAkKeyPairBlob (
    tpmclient::Client& client,
    BlobCache& blobCache,
    const bool isEnrolmentActive)
{
    try
    {
        ::std::optional<::BlobCache::Entry> entry = {};
        try
        {
            entry = blobCache.getBlob(::BlobType::AttestationKeyPair);
        }
        catch (...)
        {
            // The attestation key pair can only be created during the enrolment process.
            // That is currently not active, so throw an exception.
            // BadRequest because during the enrolment process       the processing context should not be open to the public
            // and calls to its VAU interface should not come       in.
            ErpExpect (isEnrolmentActive, ::HttpStatus::BadRequest, "attestation key pair is missing and can not be created");

            // From here on we can assume that an enrolment is ongoing. That means that calls come in only from a single thread
            // in a single process (which services the enrolment api). Locking of the database is not necessary as collisions
            // between multiple calls that all try to create a new attestation token should not be possible.

            // Create a new attestation key pair blob.
            TLOG(INFO) << "creating a new attestation token";
            ::BlobCache::Entry newEntry;
            newEntry.type = ::BlobType::AttestationKeyPair;

            TVLOG(logLevel) << "TPM: creating attestation key pair blob";
            newEntry.blob.data = SafeString(client.createAk());
            newEntry.blob.generation = 1;
            TVLOG(logLevel) << "TPM: got attestation key pair blob of size " << newEntry.blob.data.size();

            const auto hash = ::Hash::sha256(newEntry.blob.data);
            newEntry.name = ::ErpVector(hash.begin(), hash.end());
            const auto id = blobCache.storeBlob(::std::move(newEntry));
            entry = blobCache.getBlob(id);
        }

        const uint8_t* p = entry->blob.data;
        return {p, p + entry->blob.data.size()};
    }
    catchLogAndRethrow("getting AK key pair blob while setting up connection to TPM");
}


Tpm::EndorsementKeyOutput TpmProduction::getEndorsementKey (void)
{
    auto client = createTpmClient(mLogLevel);

    EndorsementKeyOutput output;
    try
    {
        const auto ek = client->getEk();
        output.certificateBase64 = Base64::encode(ek.certificate);
        output.ekNameBase64 = Base64::encode(std::string_view(reinterpret_cast<const char*>(ek.keyName.data()), ek.keyName.size()));
    }
    catchLogAndRethrow("getting EK while setting up connection to TPM");
    std::cout << std::flush;
    TVLOG(logLevel) << "successfully got EK from TPM";

    return output;
}


Tpm::AttestationKeyOutput TpmProduction::getAttestationKey (
    const bool isEnrolmentActive)
{
    auto client = createTpmClient(mLogLevel);

    auto akKeyPairBlob = provideAkKeyPairBlob(*client, mBlobCache, isEnrolmentActive);

    AttestationKeyOutput output;
    try
    {
        const auto ak = client->getAk(akKeyPairBlob);
        output.publicKeyBase64 = Base64::encode(util::rawToBuffer(ak.key.data(), ak.key.size()));
        output.akNameBase64 = Base64::encode(std::string_view(reinterpret_cast<const char*>(ak.name.data()), ak.name.size()));
    }
    catchLogAndRethrow("getting AK while setting up connection to TPM");
    std::cout << std::flush;
    if (mLogLevel >= 0)
        TVLOG(mLogLevel) << "successfully got AK from TPM";

    return output;
}


Tpm::AuthenticateCredentialOutput TpmProduction::authenticateCredential (
    AuthenticateCredentialInput&& input,
    const bool isEnrolmentActive)
{
    auto client = createTpmClient(mLogLevel);

    auto akKeyPairBlob = provideAkKeyPairBlob(*client, mBlobCache, isEnrolmentActive);

    const auto secret = guardedBase64Decode(input.secretBase64);
    const auto credential = guardedBase64Decode(input.credentialBase64);
    const auto akName = guardedBase64Decode(input.akNameBase64);

    tpmclient::PublicKeyName ak;
    Expect(akName.size() == ak.size(), "attestation key name has wrong length");
    std::copy(akName.begin(), akName.end(), ak.begin());

    tpmclient::Buffer plainTextCredential;
    try
    {
        plainTextCredential = client->authenticateCredential(
            tpmclient::BufferView(secret.data(), secret.size()),
            tpmclient::BufferView(credential.data(), credential.size()),
            akKeyPairBlob);
    }
    catchLogAndRethrow("authenticating TPM credential");
    std::cout << std::flush;
    if (mLogLevel >= 0)
        TVLOG(mLogLevel) << "TPM successfully authenticated credential";

    AuthenticateCredentialOutput output;
    output.plainTextCredentialBase64 = Base64::encode(plainTextCredential);

    return output;
}


Tpm::QuoteOutput TpmProduction::getQuote (
    QuoteInput&& input,
    const bool isEnrolmentActive)
{
    auto client = createTpmClient(mLogLevel);

    auto akKeyPairBlob = provideAkKeyPairBlob(*client, mBlobCache, isEnrolmentActive);

    const auto nonce = guardedBase64Decode(input.nonceBase64);
    ErpExpect(input.hashAlgorithm == "SHA256", HttpStatus::BadRequest, "only 'SHA256' is a supported hash algorithm");

    tpmclient::Quote quote;
    try
    {
        quote = client->getQuote(
            tpmclient::BufferView(nonce.data(), nonce.size()),
            input.pcrSet,
            akKeyPairBlob);
    }
    catchLogAndRethrow("getting quote");
    std::cout << std::flush;
    if (mLogLevel >= 0)
        TVLOG(mLogLevel) << "TPM successfully returned quote";

    QuoteOutput output;
    output.quotedDataBase64 = Base64::encode(quote.data);
    output.quoteSignatureBase64 = Base64::encode(quote.signature);

    return output;
}
