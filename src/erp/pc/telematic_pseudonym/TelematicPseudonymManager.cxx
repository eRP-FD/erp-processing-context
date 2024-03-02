/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/telematic_pseudonym/TelematicPseudonymManager.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/util/Expect.hxx"

std::unique_ptr<TelematicPseudonymManager> TelematicPseudonymManager::create(PcServiceContext* serviceContext)
{
    auto instance = std::make_unique<TelematicPseudonymManager>(serviceContext);
    instance->LoadCmacs(std::chrono::time_point_cast<date::days>(std::chrono::system_clock::now()));
    // close the unnecessary connection on the main thread:
    instance->mServiceContext->databaseFactory()->closeConnection();
    return instance;
}

TelematicPseudonymManager::TelematicPseudonymManager(PcServiceContext* serviceContext)
    : PreUserPseudonymManager(serviceContext)
{
}

void TelematicPseudonymManager::LoadCmacs(const date::sys_days& forDay)
{
    using namespace date;

    TVLOG(1) << "Loading CMAC for TelematicPseudonym from Database " << forDay;
    auto database = mServiceContext->databaseFactory();

    mKeys.clear();
    mKeys.reserve(keyHistoryLength);

    const auto today = year_month_day(forDay);
    const auto quarter = (static_cast<unsigned>(today.month()) + 2) / 3;

    // Beginning of quarter - key1 is valid.
    mKey1Start = year_month_day(day{1} / month{quarter * 3 - 2} / today.year());
    // End of quarter - key1 and key2 are valid for last day of quarter and first day of next quarter.
    mKey1End = year_month_day(last / month{quarter * 3} / today.year());

    // Beginning of next quarter - key1 and key2 are valid for last day of previous quarter and this quarter.
    mKey2Start = mKey1End;
    mKey2Start = sys_days{mKey2Start} + days{1};
    mKey2End = mKey1End;
    mKey2End += months{3};
    // End of next quarter - key2 applies (and the next key1.)
    mKey2End = year_month_day(last / mKey2End.month() / mKey2End.year());

    mKeys.push_back(database->acquireCmac(mKey1Start, CmacKeyCategory::telematic));
    mKeys.push_back(database->acquireCmac(mKey2Start, CmacKeyCategory::telematic));

    database->commitTransaction();
}

void TelematicPseudonymManager::ensureKeysUptodateForDay(date::year_month_day day)
{
    std::shared_lock lock{mMutex};
    ensureKeysUptodateForDay(lock, day);
}

bool TelematicPseudonymManager::withinGracePeriod(date::year_month_day currentDate) const
{
    using namespace date;
    auto gracePeriodEndDay = sys_days{mKey1End} + days{1};
    return (currentDate > mKey1End && currentDate <= gracePeriodEndDay);
}

bool TelematicPseudonymManager::keyUpdateRequired(date::year_month_day expirationDate) const
{
    return (expirationDate > mKey1End);
}

void TelematicPseudonymManager::ensureKeysUptodate(std::shared_lock<std::shared_mutex>& sharedLock)
{
    const auto tNow = std::chrono::system_clock::now();
    const auto today = date::year_month_day(std::chrono::time_point_cast<date::days>(tNow));
    ensureKeysUptodateForDay(sharedLock, today);
}

void TelematicPseudonymManager::ensureKeysUptodateForDay(std::shared_lock<std::shared_mutex>& sharedLock,
                                                         date::year_month_day day)
{
    A_22383.start("Update key(s) if older than expiration date.");
    if (keyUpdateRequired(day))
    {
        try
        {
            sharedLock.unlock();
            std::unique_lock lock{mMutex};
            if (keyUpdateRequired(day))
            {
                LoadCmacs(day);
            }
        }
        catch (...)
        {
            sharedLock.lock();
            throw;
        }
        sharedLock.lock();
    }
    A_22383.finish();
}
