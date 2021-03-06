/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmSession.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmEciesCurveMismatchException.hxx"
#include "erp/hsm/HsmSessionExpiredException.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/String.hxx"

#if WITH_HSM_TPM_PRODUCTION > 0
#include "erp/hsm/production/HsmProductionClient.hxx"
#endif

namespace
{
    constexpr size_t maxRetryCount = 1;

    ErpVector concat (const ErpVector& data, const OptionalDeriveKeyData& secondCallData)
    {
        ErpVector result;
        const auto& salt = secondCallData.salt;
        result.reserve(data.size() + salt.size());
        std::copy(data.begin(), data.end(), std::back_inserter(result));
        std::copy(salt.begin(), salt.end(), std::back_inserter(result));
        return result;
    }

    template<class ResultType>
    ResultType guardedRun (
        HsmSession& session,
        std::function<ResultType()>&& action)
    {
        using namespace std::string_literals;
        size_t retryCount = 0; // 0 being the original run, i.e. a try, not a re-try.
        std::exception_ptr lastException;
        while(retryCount <= maxRetryCount)
        {
            try
            {
                return action();
            }
            catch(const HsmSessionExpiredException& e)
            {
                // Failure due to an expired HSM connection. Try again.
                JsonLog(LogId::HSM_WARNING)
                    .message("HSM connection expired: "s + e.what()
#if WITH_HSM_TPM_PRODUCTION > 0
                             + " (" + HsmProductionClient::hsmErrorDetails(e.errorCode) + ")"
#endif
                             )
                    .keyValue("retry", retryCount);
                ++retryCount;

                // The actual reconnect.
                session.reconnect();
                lastException = std::current_exception();
            }
            catch(const HsmEciesCurveMismatchException&)
            {
                // No need to retry here. We gave a non-matching input.
                throw;
            }
            catch(const HsmException& e)
            {
                // Some other failure (not an expired HSM connection). Try again.
                // This catch may be removed later.
                JsonLog(LogId::HSM_WARNING)
                    .message("HSM error: "s + e.what()
#if WITH_HSM_TPM_PRODUCTION > 0
                             + " (" + HsmProductionClient::hsmErrorDetails(e.errorCode) + ")"
#endif
                             )
                    .keyValue("retry", retryCount);
                ++retryCount;

                // The actual reconnect.
                session.reconnect();
                lastException = std::current_exception();
            }
        }
        JsonLog(LogId::HSM_CRITICAL)
            .message("HSM connection expired, all retries failed")
            .keyValue("maxRetryCount", maxRetryCount);
        if (lastException)
        {
            rethrow_exception(lastException);
        }
        Fail("HSM action failed due to expired session, retry count exceeded");
    }
}


HsmSession::HsmSession (
    HsmClient& client,
    BlobCache& blobCache,
    std::shared_ptr<HsmRawSession>&& rawSession)
    : mClient(client),
      mBlobCache(blobCache),
      mTeeToken(),
      mRawSession(std::move(rawSession)),
      mLastHsmCall() // Initialized with start of epoch, interpreted as "never".
{
}

::Nonce HsmSession::getNonce(uint32_t input)
{
    return guardedRun<::Nonce>(*this, [&] {
        markHsmCallTime();
        return mClient.getNonce(*mRawSession, input);
    });
}

void HsmSession::setTeeToken (const ErpBlob& teeToken)
{
    mTeeToken = teeToken;
}


DeriveKeyOutput HsmSession::deriveTaskPersistenceKey (
    const ErpVector& derivationData,
    const std::optional<OptionalDeriveKeyData>& secondCallData)
{
    return guardedRun<DeriveKeyOutput>(*this, [&]{
          DeriveKeyInput input = makeDeriveKeyInput(BlobType::TaskKeyDerivation, derivationData, secondCallData);

          markHsmCallTime();
          return mClient.deriveTaskKey(*mRawSession, std::move(input));
    });
}


DeriveKeyOutput HsmSession::deriveCommunicationPersistenceKey (
    const ErpVector& derivationData,
    const std::optional<OptionalDeriveKeyData>& secondCallData)
{
    return guardedRun<DeriveKeyOutput>(*this, [&]{
        DeriveKeyInput input = makeDeriveKeyInput(BlobType::CommunicationKeyDerivation, derivationData, secondCallData);

        markHsmCallTime();

        // Support for old entries with task derivation keys REQUIRES that exactly the same derivation method
        // is used as at creation time.
        switch(input.blobType)
        {
        case ::BlobType::CommunicationKeyDerivation:
            return mClient.deriveCommsKey(*mRawSession, std::move(input));
        case ::BlobType::TaskKeyDerivation:
            return mClient.deriveTaskKey(*mRawSession, std::move(input));
        default:
            ErpFail(::HttpStatus::InternalServerError, ::std::string{"communication key has unexpected type "}.append(::magic_enum::enum_name(input.blobType)));
        }
    });
}


DeriveKeyOutput HsmSession::deriveAuditLogPersistenceKey (
    const ErpVector& derivationData,
    const std::optional<OptionalDeriveKeyData>& secondCallData)
{
    return guardedRun<DeriveKeyOutput>(*this, [&]{
        DeriveKeyInput input = makeDeriveKeyInput(BlobType::AuditLogKeyDerivation, derivationData, secondCallData);

        markHsmCallTime();
        return mClient.deriveAuditKey(*mRawSession, std::move(input));
    });
}


BlobId HsmSession::getLatestTaskPersistenceId (void) const
{
    return mBlobCache.getBlob(BlobType::TaskKeyDerivation).id;
}


BlobId HsmSession::getLatestCommunicationPersistenceId (void) const
{
    return mBlobCache.getBlob(BlobType::CommunicationKeyDerivation).id;
}


BlobId HsmSession::getLatestAuditLogPersistenceId (void) const
{
    return mBlobCache.getBlob(BlobType::AuditLogKeyDerivation).id;
}


ErpVector HsmSession::getRandomData (const size_t numberOfRandomBytes)
{
    Expect(numberOfRandomBytes <= MaxRndBytes, "HSM can not provide so many random bytes");

    return guardedRun<ErpVector>(*this, [&]{
           markHsmCallTime();
           return mClient.getRndBytes(
            *mRawSession,
            numberOfRandomBytes);
    });
}


ErpArray<Aes128Length> HsmSession::vauEcies128 (const ErpVector& clientPublicKey, bool useFallback)
{
    const auto get = [this, clientPublicKey](const ::ErpBlob &eciesKey) {
        return guardedRun<::ErpArray<::Aes128Length>>(*this, [&]{
            ::DoVAUECIESInput input;
            input.teeToken = getTeeToken();
            input.eciesKeyPair = eciesKey;
            input.clientPublicKey = clientPublicKey;

            markHsmCallTime();
            return mClient.doVauEcies128(
                *mRawSession,
                ::std::move(input));
        });
    };

    const auto eciesKeys = mBlobCache.getEciesKeys();

    if (! useFallback)
    {
        try
        {
            return get(eciesKeys.latest.blob);
        }
        catch (const ::HsmEciesCurveMismatchException&)
        {
            // ignore error and retry with fallback
        }
    }

    ErpExpect(eciesKeys.fallback, ::HttpStatus::InternalServerError, "No fallback key available");
    return get(eciesKeys.fallback->blob);
}


std::tuple<shared_EVP_PKEY, ErpBlob> HsmSession::getVauSigPrivateKey (const shared_EVP_PKEY& cachedKey,
                                                                      const ErpBlob& cachedBlob)
{
    auto blob = mBlobCache.getBlob(BlobType::VauSig).blob;
    if (cachedKey.isSet() && cachedBlob == blob)
    {
        return std::make_tuple(cachedKey, cachedBlob);
    }
    return guardedRun<std::tuple<shared_EVP_PKEY, ErpBlob>>(*this, [&]{
        GetVauSigPrivateKeyInput input;
        input.teeToken = getTeeToken();
        input.vauSigPrivateKey = blob;

        markHsmCallTime();
        auto privateKeyPkcs8Der = mClient.getVauSigPrivateKey(
            *mRawSession,
            std::move(input));

        shared_EVP_PKEY privateKey;
        const unsigned char* p = privateKeyPkcs8Der;
        auto* privateKeyPointer = d2i_PrivateKey(
            EVP_PKEY_EC,
            privateKey.getP(),
            &p,
            gsl::narrow<long>(privateKeyPkcs8Der.size()));
        ErpExpect(privateKeyPointer != nullptr, HttpStatus::InternalServerError, "conversion of VAU SIG private key failed");

        return std::make_tuple(privateKey, blob);
    });
}

Certificate HsmSession::getVauSigCertificate() const
{
    auto cert = mBlobCache.getBlob(BlobType::VauSig).certificate;
    if (! cert.has_value() || cert.value().empty())
    {
        cert = Configuration::instance().getOptionalStringValue(ConfigurationKey::C_FD_SIG_ERP);
        TLOG(WARNING) << "using C.FD.SIG VauSig Certificate from configuration file/environment";
    }
    Expect(cert.has_value() && ! cert.value().empty(),
           "VauSig Certificate neither in blob cache nor in configuration file/environment");
    return Certificate::fromBase64(*cert);
}


ErpArray<Aes256Length> HsmSession::getKvnrHmacKey (void)
{
    return guardedRun<ErpArray<Aes256Length>>(*this, [&]{
        UnwrapHashKeyInput input;
        input.teeToken = getTeeToken();
        input.key = mBlobCache.getBlob(BlobType::KvnrHashKey).blob;

        markHsmCallTime();
        return mClient.unwrapHashKey(
            *mRawSession,
            std::move(input));
    });
}


ErpArray<Aes256Length> HsmSession::getTelematikIdHmacKey (void)
{
    return guardedRun<ErpArray<Aes256Length>>(*this, [&]{
        UnwrapHashKeyInput input;
        input.teeToken = getTeeToken();
        input.key = mBlobCache.getBlob(BlobType::TelematikIdHashKey).blob;

        markHsmCallTime();
        return mClient.unwrapHashKey(
            *mRawSession,
            std::move(input));
    });
}

::ParsedQuote HsmSession::parseQuote(const ::ErpVector& quote) const
{
    return mClient.parseQuote(quote);
}

DeriveKeyInput HsmSession::makeDeriveKeyInput(
    const BlobType blobType,
    const ErpVector& derivationData,
    const std::optional<OptionalDeriveKeyData>& secondCallData)
{
    Expect(derivationData.size() <= MaxBuffer, "invalid derivation data");

    DeriveKeyInput input;
    input.teeToken = getTeeToken();
    input.blobType = blobType;
    auto ak = mBlobCache.getBlob(BlobType::AttestationPublicKey);
    Expect(ak.name.size() == TpmObjectNameLength, "ak name has the wrong size");
    std::copy(ak.name.begin(), ak.name.end(), input.akName.begin());
    if (secondCallData.has_value())
    {
        input.initialDerivation = false;
        if (blobType == ::BlobType::CommunicationKeyDerivation)
        {
            // The special handling of communication keys is due to bug ERP-9023.
            // Some databases may still use task keys for older communication entries, so
            // we use a generic lookup by id, ignoring the exact key type.

            const auto key = mBlobCache.getBlob(secondCallData->blobId);

            ErpExpect(
                (key.type == ::BlobType::CommunicationKeyDerivation) || (key.type == ::BlobType::TaskKeyDerivation),
                HttpStatus::InternalServerError,
                ::std::string{"communication key has unexpected type "}.append(::magic_enum::enum_name(key.type)));

            input.derivationKey = key.blob;
            input.blobType = key.type;
        }
        else
        {
            input.derivationKey = mBlobCache.getBlob(blobType, secondCallData->blobId).blob;
        }

        input.derivationData = concat(derivationData, secondCallData.value());
        input.blobId = secondCallData->blobId;
    }
    else
    {
        input.initialDerivation = true;
        const auto& entry = mBlobCache.getBlob(blobType);
        input.derivationKey = entry.blob;
        input.blobId = entry.id;
        input.derivationData = derivationData;
    }

    return input;
}


const ErpBlob& HsmSession::getTeeToken (void) const
{
    if (mTeeToken.data.size() == 0)
        TLOG(WARNING) << "tee token is empty";
    return mTeeToken;
}


void HsmSession::keepAlive (std::chrono::system_clock::time_point threshold)
{
    if (mLastHsmCall < threshold)
    {
        // Temporarily log the keep alive update.
        // Remove latest after June, 20th.
        TLOG(WARNING) << "keeping hsm session alive which was last accessed "
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now() - mLastHsmCall).count()
                 << " milliseconds ago at " << model::Timestamp(mLastHsmCall).toXsDateTime();

        (void)getRandomData(1);
    }
}


void HsmSession::markHsmCallTime (void)
{
    mLastHsmCall = std::chrono::system_clock::now();
}



void HsmSession::reconnect (void)
{
    mClient.reconnect(*mRawSession);
}

