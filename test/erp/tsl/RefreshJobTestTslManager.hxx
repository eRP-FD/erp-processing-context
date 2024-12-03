/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_REFRESHJOBTESTTSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_REFRESHJOBTESTTSLMANAGER_HXX

#include "shared/tsl/TslManager.hxx"

#include <memory>
#include <vector>
#include <exception>

class UrlRequestSender;
class XmlValidator;
class TrustStore;

class TestException : public std::exception
{
};

class RefreshJobTestTslManager : public TslManager
{
public:
    RefreshJobTestTslManager(
        std::shared_ptr<UrlRequestSender> requestSender,
        std::shared_ptr<XmlValidator> xmlValidator,
        const std::vector<std::function<void(void)>>& initialPostUpdateHooks,
        std::unique_ptr<TrustStore> tslTrustStore,
        std::unique_ptr<TrustStore> bnaTrustStore)
        : TslManager(std::move(requestSender), std::move(xmlValidator), initialPostUpdateHooks,
                     std::move(tslTrustStore), std::move(bnaTrustStore))
        , updateCount(0)
    {
    }

    ~RefreshJobTestTslManager() override = default;

    void updateTrustStoresOnDemand() override
    {
        updateCount++;
        TslManager::updateTrustStoresOnDemand();
    }

    std::atomic<int> updateCount;
};

#endif
