/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/DiffieHellman.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "erp/pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/Uuid.hxx"

#include <exception>
#include <mutex>

namespace
{
// Avoid HSM access by memorizing the key. Access to this variable
// from outside the class is not possible and keeping a backup
// within the TEE (VAU) is allowed according to A_22698.1.
// Furthermore, it provides a convenient way to handle a general
// error condition in ::hkdf.
SafeString sKey{};
}

PseudonameKeyRefreshJob::PseudonameKeyRefreshJob(HsmPool& hsmPool, BlobCache& blobCache,
                                                 const std::chrono::steady_clock::duration checkInterval,
                                                 const std::chrono::steady_clock::duration failedCheckInterval,
                                                 const std::chrono::steady_clock::duration expireInterval)
    : TimerJobBase("PseudonameKeyRefreshJob", failedCheckInterval)
    , mHsmPool(hsmPool)
    , mBlobCache(blobCache)
    , mCheckInterval(checkInterval)
    , mFailedCheckInterval(failedCheckInterval)
    , mExpireInterval(expireInterval)
{
    PseudonameKeyRefreshJob::executeJob();
}

void PseudonameKeyRefreshJob::onStart()
{
}

BlobId PseudonameKeyRefreshJob::createAndStoreKey()
{
    BlobDatabase::Entry dbEntry;
    dbEntry.type = BlobType::PseudonameKey;
    Uuid uuid;
    std::string s{"pseudoname_" + uuid.toString()};
    dbEntry.name = ErpVector::create(s);
    TVLOG(1) << "PseudonameKeyRefreshJob create new key for pseudoname";
    using namespace std::chrono;
    //  Key shall expire from tNow + REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS.
    dbEntry.expiryDateTime = time_point_cast<milliseconds>(system_clock::now() + mExpireInterval);
    auto hsmPoolSession = mHsmPool.acquire();
    A_22698.start("#1 Let the HSM generate the key.");
    dbEntry.blob = hsmPoolSession.session().generatePseudonameKey(0);// Ask for the latest generation in HSM.
    BlobId id = mBlobCache.storeBlob(std::move(dbEntry));
    A_22698.finish();
    A_22698.start("#2 Only TEE/VAU has access to this key.");
    ErpArray<Aes256Length> key = hsmPoolSession.session().unwrapPseudonameKey();
    sKey = SafeString{key.data(), key.size()};
    A_22698.finish();
    return id;
}

void PseudonameKeyRefreshJob::executeJob()
{
    A_22698.start("#3 Check if key needs a refresh, otherwise provide existing key.");
    // This code is executed once per REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS, when the last check succeeded
    // or REPORT_LEIPS_KEY_FAILED_CHECK_INTERVAL_SECONDS, while the check fails.
    try
    {
        TLOG(INFO) << "Running PseudonameKeyRefreshJob";
        std::vector<bool> results = mBlobCache.hasValidBlobsOfType({BlobType::PseudonameKey});
        if (results.at(0) == false)
        {
            // Existing key expired. Create new key and store it, remove previous key.
            TLOG(INFO) << "PseudonameKeyRefreshJob add missing OR replace expired key for pseudoname";
            const BlobId id = createAndStoreKey();
            const std::vector<BlobDatabase::Entry> blobs = mBlobCache.getAllBlobsSortedById();
            for (const BlobDatabase::Entry& blob : blobs)
            {
                if (blob.type == BlobType::PseudonameKey && blob.id != id)
                {
                    TLOG(INFO) << "PseudonameKeyRefreshJob remove expired key";
                    mBlobCache.deleteBlob(blob.type, blob.name);
                }
            }
        }
        else
        {
            auto hsmPoolSession = mHsmPool.acquire();
            ErpArray<Aes256Length> key = hsmPoolSession.session().unwrapPseudonameKey();
            SafeString currentKey{sKey};
            sKey = SafeString{key.data(), key.size()};
            if (currentKey == sKey)
            {
                TLOG(INFO) << "PseudonameKeyRefreshJob loaded existing key";
            }
            else
            {
                TLOG(INFO) << "PseudonameKeyRefreshJob loaded new key";
            }
        }
        setInterval(mCheckInterval);
    }
    catch (...)
    {
        setInterval(mFailedCheckInterval);
        sKey = SafeString("");
        // Working with the BlobCache (and indirectly with BlobDatabase and HsmClient) may fail.
        // Catch any exception to make sure the server continues its operation even if this
        // part fails.
        // Log the error and try again at the next interval.
        try
        {
            std::rethrow_exception(std::current_exception());
        }
        catch (const std::exception& exception)
        {
            JsonLog(LogId::HSM_WARNING, JsonLog::makeWarningLogReceiver())
                .message("PseudonameKeyRefreshJob failed")
                .details(exception.what());
        }
    }
    A_22698.finish();
}


void PseudonameKeyRefreshJob::onFinish()
{
    sKey = SafeString("");
}

std::unique_ptr<PseudonameKeyRefreshJob>
PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(HsmPool& hsmPool, BlobCache& blobCache,
                                                      const Configuration& configuration)
{
    if (configuration.getBoolValue(ConfigurationKey::REPORT_LEIPS_KEY_ENABLE))
    {
        const std::chrono::seconds expireInterval{
            configuration.getIntValue(ConfigurationKey::REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS)};
        const std::chrono::seconds refreshInterval{
            configuration.getIntValue(ConfigurationKey::REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS)};
        const std::chrono::seconds failedCheckInterval{
            configuration.getIntValue(ConfigurationKey::REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS)};
        std::unique_ptr<PseudonameKeyRefreshJob> refreshJob = std::make_unique<PseudonameKeyRefreshJob>(
            hsmPool, blobCache, refreshInterval, failedCheckInterval, expireInterval);
        refreshJob->start();
        return refreshJob;
    }
    return {};
}

std::string PseudonameKeyRefreshJob::hkdf(const std::string& input)
{
    A_22698.start("#4 Generate hash.");
    if (! sKey.size())
    {
        return "";
    }
    std::string result = Base64::encode(DiffieHellman::hkdf(sKey, input, AesGcm256::KeyLength));
    A_22698.finish();
    return result;
}
