/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_PREUSERPSEUDONYM_PREUSERPSEUDONYMMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_PREUSERPSEUDONYM_PREUSERPSEUDONYMMANAGER_HXX

#include "erp/crypto/CMAC.hxx"

#include <date/date.h>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <tuple>
#include <vector>

class PcServiceContext;

class PreUserPseudonymManager
{
public:
    static constexpr std::string_view PNPHeader{"PNP"};

    virtual ~PreUserPseudonymManager() = default;

    [[nodiscard]]
    static std::unique_ptr<PreUserPseudonymManager> create(PcServiceContext* serviceContext);

    /// @returns {verified, updatedSignature}
    [[nodiscard]]
    std::tuple<bool, CmacSignature> verifyAndReSign(const CmacSignature& sig, const std::string_view& subClaim);

    [[nodiscard]]
    virtual CmacSignature sign(const std::string_view& subClaim);

    PreUserPseudonymManager(const PreUserPseudonymManager&) = delete;
    PreUserPseudonymManager(PreUserPseudonymManager&&) = delete;
    PreUserPseudonymManager& operator = (const PreUserPseudonymManager&) = delete;
    PreUserPseudonymManager& operator = (PreUserPseudonymManager&&) = delete;
    [[nodiscard]]
    CmacSignature sign(size_t keyNr, const std::string_view& subClaim);
protected:
    static constexpr size_t keyHistoryLength = 2;

    explicit PreUserPseudonymManager(PcServiceContext* serviceContext);
    virtual void LoadCmacs(const date::sys_days& forDay);
    virtual void ensureKeysUptodate(std::shared_lock<std::shared_mutex>& sharedLock);

    PcServiceContext* const mServiceContext;

    date::sys_days mKeyDate;

    std::shared_mutex mMutex;
    // mCmacDate belongs to mCmacs[0]
    // mCmac[1] belongs to the day before ...
    std::vector<CmacKey> mKeys;

#ifdef FRIEND_TEST
    FRIEND_TEST(PreUserPseudonymCmacTest, ERP6504RecoverAfterFailure);
#endif
};


#endif
