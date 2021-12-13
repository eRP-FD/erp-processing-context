/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_TELEMATICPSEUDONYM_TELEMATICPSEUDONYMMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_TELEMATICPSEUDONYM_TELEMATICPSEUDONYMMANAGER_HXX

#include "erp/crypto/CMAC.hxx"

#include <date/date.h>
#include <memory>
#include <shared_mutex>
#include <string_view>

class PcServiceContext;

class TelematicPseudonymManager
{
    static constexpr size_t keyHistoryLength = 2;

public:
    [[nodiscard]] static std::unique_ptr<TelematicPseudonymManager> create(PcServiceContext& serviceContext);
    explicit TelematicPseudonymManager(PcServiceContext& serviceContext);

    /// @returns {verified, updatedSignature}
    [[nodiscard]] std::tuple<bool, CmacSignature> verifyAndReSign(const CmacSignature& sig,
                                                                  const std::string_view& subClaim);

    [[nodiscard]] CmacSignature sign(const std::string_view& src);
    [[nodiscard]] CmacSignature sign(size_t keyNr, const std::string_view& src);

    bool withinGracePeriod(date::year_month_day currentDate) const;
    bool keyUpdateRequired(date::year_month_day expirationDate) const;

    TelematicPseudonymManager(const TelematicPseudonymManager&) = delete;
    TelematicPseudonymManager(TelematicPseudonymManager&&) = delete;
    TelematicPseudonymManager& operator=(const TelematicPseudonymManager&) = delete;
    TelematicPseudonymManager& operator=(TelematicPseudonymManager&&) = delete;

    void LoadCmacs(const date::sys_days& forDay);
    void ensureKeysUptodateForDay(date::year_month_day day);
    std::tuple<date::year_month_day, date::year_month_day> getKey1Range() { return { mKey1Start, mKey1End }; }
    std::tuple<date::year_month_day, date::year_month_day> getKey2Range() { return { mKey2Start, mKey2End }; }

private:
    void ensureKeysUptodate(std::shared_lock<std::shared_mutex>& sharedLock);
    void ensureKeysUptodateForDay(std::shared_lock<std::shared_mutex>& sharedLock, date::year_month_day day);

    PcServiceContext& mServiceContext;
    std::shared_mutex mMutex;
    std::vector<CmacKey> mKeys;
    date::year_month_day mKey1Start;
    date::year_month_day mKey1End;
    date::year_month_day mKey2Start;
    date::year_month_day mKey2End;
};

#endif
