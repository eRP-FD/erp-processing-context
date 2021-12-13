/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmSession.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmSessionExpiredException.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/JsonLog.hxx"

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
        size_t retryCount = 0; // 0 being the original run, i.e. a try, not a re-try.
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
                    .message("HSM connection expired: "
#if WITH_HSM_TPM_PRODUCTION > 0
                             + HsmProductionClient::hsmErrorDetails(e.errorCode)
#endif
                             )
                    .keyValue("retry", retryCount);
                ++retryCount;

                // The actual reconnect.
                session.reconnect();
            }
            catch(const HsmException& e)
            {
                // Some other failure (not an expired HSM connection). Try again.
                // This catch may be removed later.
                JsonLog(LogId::HSM_WARNING)
                    .message("HSM error: "
#if WITH_HSM_TPM_PRODUCTION > 0
                             + HsmProductionClient::hsmErrorDetails(e.errorCode)
#endif
                             )
                    .keyValue("retry", retryCount);
                ++retryCount;

                // The actual reconnect.
                session.reconnect();
            }
        }
        JsonLog(LogId::HSM_CRITICAL)
            .message("HSM connection expired, all retries failed")
            .keyValue("retry", retryCount);
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
        return mClient.deriveCommsKey(*mRawSession, std::move(input));
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


ErpArray<Aes128Length> HsmSession::vauEcies128 (const ErpVector& clientPublicKey)
{
    return guardedRun<ErpArray<Aes128Length>>(*this, [&]{
        DoVAUECIESInput input;
        input.teeToken = getTeeToken();
        input.eciesKeyPair = mBlobCache.getBlob(BlobType::EciesKeypair).blob;
        input.clientPublicKey = clientPublicKey;

        markHsmCallTime();
        return mClient.doVauEcies128(
            *mRawSession,
            std::move(input));
    });
}


shared_EVP_PKEY HsmSession::getVauSigPrivateKey (void)
{
    return guardedRun<shared_EVP_PKEY>(*this, [&]{
        GetVauSigPrivateKeyInput input;
        input.teeToken = getTeeToken();
        input.vauSigPrivateKey = mBlobCache.getBlob(BlobType::VauSigPrivateKey).blob;

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

        return privateKey;
    });
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


DeriveKeyInput HsmSession::makeDeriveKeyInput(
    const BlobType blobType,
    const ErpVector& derivationData,
    const std::optional<OptionalDeriveKeyData>& secondCallData)
{
    Expect(derivationData.size() <= MaxBuffer, "invalid derivation data");

    DeriveKeyInput input;
    input.teeToken = getTeeToken();
    auto ak = mBlobCache.getBlob(BlobType::AttestationPublicKey);
    Expect(ak.name.size() == TpmObjectNameLength, "ak name has the wrong size");
    std::copy(ak.name.begin(), ak.name.end(), input.akName.begin());
    if (secondCallData.has_value())
    {
        input.initialDerivation = false;
        input.derivationKey = mBlobCache.getBlob(blobType, secondCallData->blobId).blob;
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

