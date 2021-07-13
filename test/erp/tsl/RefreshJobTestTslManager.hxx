#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_REFRESHJOBTESTTSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_REFRESHJOBTESTTSLMANAGER_HXX

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
        const std::vector<std::function<void(void)>> initialPostUpdateHooks,
        std::unique_ptr<TrustStore> tslTrustStore,
        std::unique_ptr<TrustStore> bnaTrustStore)
        : TslManager(
        requestSender, xmlValidator, initialPostUpdateHooks, std::move(tslTrustStore), std::move(bnaTrustStore))
        , updateCount(0)
    {
    }

    virtual ~RefreshJobTestTslManager() = default;

    virtual void updateTrustStoresOnDemand()
    {
        updateCount++;
        TslManager::updateTrustStoresOnDemand();
    }

    std::atomic<int> updateCount;
};

#endif
