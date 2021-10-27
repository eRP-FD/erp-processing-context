/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"

#include "erp/database/Database.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"


std::unique_ptr<PreUserPseudonymManager> PreUserPseudonymManager::create(PcServiceContext* serviceContext)
{
    std::unique_ptr<PreUserPseudonymManager> instance{new PreUserPseudonymManager(serviceContext)};
    return instance;
}

PreUserPseudonymManager::PreUserPseudonymManager(PcServiceContext* serviceContext)
    : mServiceContext(serviceContext)
{
    Expect(mServiceContext != nullptr, "serviceContext must not be NULL.");
    LoadCmacs(std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now()));
    // close the unnecessary connection on the main thread:
    mServiceContext->databaseFactory()->closeConnection();
}

void PreUserPseudonymManager::LoadCmacs(const date::sys_days& forDay)
{
    TVLOG(1) << "Loading CMAC for PreUserPseudonym from Database";
    auto database = mServiceContext->databaseFactory();
    mKeyDate = forDay;
    mKeys.clear();
    mKeys.reserve(keyHistoryLength);
    for (size_t i = 0; i < keyHistoryLength; ++i)
    {
        mKeys.push_back(database->acquireCmac(mKeyDate - date::days{i}));
    }
    database->commitTransaction();
}

CmacSignature PreUserPseudonymManager::sign(const std::string_view& subClaim)
{
    std::shared_lock lock{mMutex};
    ensureKeysUptodate(lock);
    return sign(0, subClaim);
}

CmacSignature PreUserPseudonymManager::sign(size_t keyNr, const std::string_view& subClaim)
{
    Expect(mKeys.size() > keyNr, "Key for day -" + std::to_string(keyNr) + " not loaded. Cannot sign subClaim.");
    return mKeys[keyNr].sign(subClaim);
}

std::tuple<bool, CmacSignature> PreUserPseudonymManager::verifyAndResign(const CmacSignature& sig, const std::string_view& subClaim)
{
    using std::get;
    std::shared_lock lock{mMutex};
    ensureKeysUptodate(lock);
    std::tuple<bool, CmacSignature> result{false,  sign(0, subClaim)};
    if (get<1>(result) == sig)
    {
        get<0>(result) = true;
        return result;
    }
    for (size_t i = 1; i < mKeys.size(); ++i)
    {
        auto sigI = sign(i, subClaim);
        if (sigI == sig)
        {
            get<0>(result) = true;
            return result;
        }
    }
    return result;
}

void PreUserPseudonymManager::ensureKeysUptodate(std::shared_lock<std::shared_mutex>& sharedLock)
{
    A_20163.start("6. Die E-Rezept-VAU MUSS einen 128-Bit-AES-CMAC-Schlüssel zufällig erzeugen und mindestens alle 10 Tage wechseln.");
    auto today = std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now());
    if (today != mKeyDate)
    {
        try {
            sharedLock.unlock();
            std::unique_lock lock{mMutex};
            if (today != mKeyDate)
            {
                LoadCmacs(today);
            }
        }
        catch(...)
        {
            sharedLock.lock();
            throw;
        }
        sharedLock.lock();
    }
    A_20163.finish();
}



