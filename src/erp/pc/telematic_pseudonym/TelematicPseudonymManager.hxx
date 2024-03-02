/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_TELEMATICPSEUDONYM_TELEMATICPSEUDONYMMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_TELEMATICPSEUDONYM_TELEMATICPSEUDONYMMANAGER_HXX

#include "erp/crypto/CMAC.hxx"
#include "erp/pc/pre_user_pseudonym/PreUserPseudonymManager.hxx"

#include <date/date.h>
#include <memory>
#include <shared_mutex>
#include <string_view>

class PcServiceContext;

class TelematicPseudonymManager : public PreUserPseudonymManager
{
public:
    [[nodiscard]] static std::unique_ptr<TelematicPseudonymManager> create(PcServiceContext* serviceContext);
    explicit TelematicPseudonymManager(PcServiceContext* serviceContext);

    bool withinGracePeriod(date::year_month_day currentDate) const;
    bool keyUpdateRequired(date::year_month_day expirationDate) const;

    TelematicPseudonymManager(const TelematicPseudonymManager&) = delete;
    TelematicPseudonymManager(TelematicPseudonymManager&&) = delete;
    TelematicPseudonymManager& operator=(const TelematicPseudonymManager&) = delete;
    TelematicPseudonymManager& operator=(TelematicPseudonymManager&&) = delete;

    void LoadCmacs(const date::sys_days& forDay) override;
    void ensureKeysUptodateForDay(date::year_month_day day);
    std::tuple<date::year_month_day, date::year_month_day> getKey1Range() { return { mKey1Start, mKey1End }; }
    std::tuple<date::year_month_day, date::year_month_day> getKey2Range() { return { mKey2Start, mKey2End }; }

private:
    void ensureKeysUptodate(std::shared_lock<std::shared_mutex>& sharedLock) override;
    void ensureKeysUptodateForDay(std::shared_lock<std::shared_mutex>& sharedLock, date::year_month_day day);

    date::year_month_day mKey1Start{};
    date::year_month_day mKey1End{};
    date::year_month_day mKey2Start{};
    date::year_month_day mKey2End{};
#ifdef FRIEND_TEST
    FRIEND_TEST(CommunicationPostEndpointTest, ERP_12846_SubscriptionKeyRefresh);
#endif
};

#endif
