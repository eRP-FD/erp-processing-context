/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/TslManager.hxx"

#include "shared/ErpRequirements.hxx"
#include "shared/tsl/OcspService.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslService.hxx"
#include "shared/tsl/X509Store.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"

#if WITH_HSM_MOCK > 0
#include "mock/tsl/MockTslManager.hxx"
#endif

namespace
{
    std::vector<std::string> getInitialTslDownloadUrls()
    {
        return {Configuration::instance().getStringValue(ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL)};
    }


    [[noreturn]]
    void handleException(std::exception_ptr exception,
                         const char* logPrefix,
                         const FileNameAndLineNumber& fileNameAndLineNumber)
    {
        try
        {
            std::rethrow_exception(std::move(exception));
        }
        catch(const TslError& e)
        {
            TLOG(ERROR) << fileNameAndLineNumber.fileName << ":" << fileNameAndLineNumber.line << ", " << logPrefix << ", TslError:";
            TLOG(ERROR) << e.what();
            throw;
        }
        catch(const std::runtime_error& e)
        {
            TLOG(ERROR) << fileNameAndLineNumber.fileName << ":" << fileNameAndLineNumber.line << ", " << logPrefix
                       << ", unexpected runtime exception: " << e.what();
            TslFail(e.what(), TslErrorCode::UNKNOWN_ERROR);
        }
        catch(const std::exception& e)
        {
            TLOG(ERROR) << fileNameAndLineNumber.fileName << ":" << fileNameAndLineNumber.line << ", " << logPrefix
                       << ", unexpected exception: " << e.what();
            throw;
        }
        catch(...)
        {
            TLOG(ERROR) << fileNameAndLineNumber.fileName << ":" << fileNameAndLineNumber.line << ", " << logPrefix
                       << ", unknown exception";
            throw;
        }
    }

    bool tryCacheFirst(OcspCheckDescriptor::OcspCheckMode mode)
    {
        switch (mode)
        {
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_STRICT:
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_ALLOW_CACHE:
            case OcspCheckDescriptor::PROVIDED_ONLY:
                return false;
            case OcspCheckDescriptor::PROVIDED_OR_CACHE:
            case OcspCheckDescriptor::CACHED_ONLY:
                return true;
        }
        Fail("invalid value for OcspCheckMode: " + std::to_string(static_cast<uintmax_t>(mode)));
    }
}


#define HANDLE_EXCEPTION(prefix) \
    handleException(std::current_exception(), prefix, fileAndLine);


std::shared_ptr<TslManager> TslManager::setupTslManager(const std::shared_ptr<XmlValidator>& xmlValidator)
{
    TLOG(INFO) << "Initializing TSL-Manager";

    try
    {
        std::string tslSslRootCa;
        const std::string tslRootCaFile =
            Configuration::instance().getStringValue(ConfigurationKey::TSL_FRAMEWORK_SSL_ROOT_CA_PATH);
        if ( ! tslRootCaFile.empty())
        {
            tslSslRootCa = FileHelper::readFileAsString(tslRootCaFile);
        }

        const auto useMockTslManager =
            Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_ENABLE_MOCK_TSL_MANAGER, false);

#if WITH_HSM_MOCK > 0
        if (useMockTslManager)
        {
            return MockTslManager::createMockTslManager(xmlValidator);
        }
#else
        Expect3(! useMockTslManager,
                "Configuration error: DEBUG_ENABLE_MOCK_TSL_MANAGER=true, but WITH_HSM_MOCK is not compiled in",
                std::logic_error);
#endif
        auto requestSender = std::make_shared<UrlRequestSender>(
            std::move(tslSslRootCa),
            static_cast<uint16_t>(
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)),
            std::chrono::milliseconds{
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});
        return std::make_shared<TslManager>(std::move(requestSender), xmlValidator);
    }
    catch (const TslError& e)
    {
        TLOG(ERROR) << "Can not create TslManager, TslError: " << e.what();
        throw;
    }
    catch (const std::exception& e)
    {
        TLOG(ERROR) << "Can not create TslManager, unexpected exception: " << e.what();
        throw;
    }
}


TslManager::TslManager(
    std::shared_ptr<UrlRequestSender> requestSender,
    std::shared_ptr<XmlValidator> xmlValidator,
    const std::vector<PostUpdateHook>& initialPostUpdateHooks)
    : mRequestSender(std::move(requestSender))
    , mXmlValidator(std::move(xmlValidator))
    , mTslTrustStore(std::make_unique<TrustStore>(TslMode::TSL, getInitialTslDownloadUrls()))
    , mBnaTrustStore(std::make_unique<TrustStore>(TslMode::BNA))
    , mUpdateHookMutex()
    , mPostUpdateHooks(initialPostUpdateHooks)
{
    try
    {
        Expect(mRequestSender != nullptr, "The request sender must not be null");
        Expect(mXmlValidator != nullptr, "The xml validator must not be null");

        GS_A_4898.start("Start initial download of TSL & BNetzA-VL.");
        internalUpdate(true);
        GS_A_4898.finish();
    }
    catch(...)
    {
        HANDLE_EXCEPTION("can not initialize TslManager");
    }
}


TslManager::TslManager(
    std::shared_ptr<UrlRequestSender> requestSender,
    std::shared_ptr<XmlValidator> xmlValidator,
    const std::vector<PostUpdateHook>& initialPostUpdateHooks,
    std::unique_ptr<TrustStore> tslTrustStore,
    std::unique_ptr<TrustStore> bnaTrustStore)
    : mRequestSender(std::move(requestSender))
    , mXmlValidator(std::move(xmlValidator))
    , mTslTrustStore(std::move(tslTrustStore))
    , mBnaTrustStore(std::move(bnaTrustStore))
    , mUpdateHookMutex()
    , mPostUpdateHooks(initialPostUpdateHooks)
{
    // this is a protected constructor that is only intended to be used for testing purposes
    try
    {
        Expect(mRequestSender != nullptr, "The request sender must not be null");
        Expect(mXmlValidator != nullptr, "The xml validator must not be null");
        Expect(mTslTrustStore != nullptr, "TSL TrustStore must be provided.");
        if (mBnaTrustStore == nullptr)
        {
            mBnaTrustStore = std::make_unique<TrustStore>(TslMode::BNA);
        }
    }
    catch(...)
    {
        HANDLE_EXCEPTION("can not initialize TslManager");
    }
}


X509Store TslManager::getTslTrustedCertificateStore(
    const TslMode tslMode,
    const std::optional<X509Certificate>& certificate)
{
    // no mutex is necessary because trust store is thread safe
    try
    {
        GS_A_4898.start("Minimize TSL-Grace-Period, check TSL-validity and try to update if invalid each time TSL-based trust store is used.");
        internalUpdate(true);
        GS_A_4898.finish();

        return getTrustStore(tslMode).getX509Store(certificate);
    }
    catch(...)
    {
        HANDLE_EXCEPTION("can not get trusted certificates store");
    }

    return {}; // not reachable
}


// GEMREQ-start A_22141#verifyCertificate, A_20159-03#verifyCertificate
void TslManager::verifyCertificate(const TslMode tslMode,
                                   const X509Certificate& certificate,
                                   const std::unordered_set<CertificateType>& typeRestrictions,
                                   const OcspCheckDescriptor& ocspCheckDescriptor)
{
    // no mutex is necessary because trust store is thread safe
    try
    {
        GS_A_4898.start("Minimize TSL-Grace-Period, check TSL-validity and try to update if invalid each time a certificate is validated.");
        internalUpdate(true);
        GS_A_4898.finish();

        TslService::checkCertificate(
            certificate,
            typeRestrictions,
            *mRequestSender,
            getTrustStore(tslMode),
            ocspCheckDescriptor);
    }
    catch(const TslError& e)
    {
        // simply rethrow in case the verification itself failed
        // to avoid logging in case of (expected) errors
        TLOG(INFO) << "certificate verification failed: " << e.what();
        throw;
    }
    catch(...)
    {
        HANDLE_EXCEPTION("certificate verification failed");
    }
}
// GEMREQ-end A_22141#verifyCertificate, A_20159-03#verifyCertificate


// GEMREQ-start A_20765-02#ocspResponse
OcspResponse
 TslManager::getCertificateOcspResponse(
    const TslMode tslMode,
    const X509Certificate& certificate,
    const std::unordered_set<CertificateType>& typeRestrictions,
    const OcspCheckDescriptor& ocspCheckDescriptor)
{
    // no mutex is necessary because trust store is thread safe
    try
    {
        std::optional<OcspResponse> ocspResponse;

        GS_A_4898.start("Minimize TSL-Grace-Period, check TSL-validity and try to update if invalid each time a certificate is validated.");
        const TslService::UpdateResult updateResult = internalUpdate(true);
        GS_A_4898.finish();

        if (updateResult == TslService::UpdateResult::NotUpdated
            && tryCacheFirst(ocspCheckDescriptor.mode)
            && ocspCheckDescriptor.providedOcspResponse == nullptr)
        {
            // the cached OCSP-Response still can be used, if it exists and is still valid
            ocspResponse = getTrustStore(tslMode).getCachedOcspData(certificate.getSha256FingerprintHex());
        }

        if (!ocspResponse.has_value())
        {
            ocspResponse.emplace(TslService::checkCertificate(certificate, typeRestrictions, *mRequestSender,
                                                              getTrustStore(tslMode), ocspCheckDescriptor));
        }
        ocspResponse->checkStatus(getTrustStore(tslMode));
        return *ocspResponse;
    }
    catch(...)
    {
        HANDLE_EXCEPTION("ocsp response retrieval failed");
    }
}
// GEMREQ-end A_20765-02#ocspResponse


void TslManager::updateTrustStoresOnDemand()
{
    // no mutex is necessary because trust store and TslService are thread safe
    try
    {
        internalUpdate(false);
    }
    catch(...)
    {
        HANDLE_EXCEPTION("can not update trust stores");
    }
}


size_t TslManager::addPostUpdateHook (const PostUpdateHook& postUpdateHook)
{
    std::lock_guard lock (mUpdateHookMutex);

    mPostUpdateHooks.emplace_back(postUpdateHook);
    return mPostUpdateHooks.size() - 1;
}


void TslManager::disablePostUpdateHook(const size_t hookId)
{
    std::lock_guard lock (mUpdateHookMutex);

    Expect(hookId < mPostUpdateHooks.size(), "unexpected hook id " + std::to_string(hookId) );
    mPostUpdateHooks[hookId] = nullptr;
}


TrustStore::HealthData TslManager::healthCheckTsl() const
{
    return mTslTrustStore->getHealthData();
}


TrustStore::HealthData TslManager::healthCheckBna() const
{
    return mBnaTrustStore->getHealthData();
}


TrustStore & TslManager::getTrustStore(const TslMode tslMode)
{
    Expect(mTslTrustStore != nullptr, "the TSL trust store pointer must be initialized");
    Expect(mBnaTrustStore != nullptr, "the BNA trust store pointer must be initialized");

    switch(tslMode)
    {
        case TSL:
            return *mTslTrustStore;

        case BNA:
            return *mBnaTrustStore;
    }

    LogicErrorFail("unexpected TslMode");
}


TslService::UpdateResult TslManager::internalUpdate(const bool onlyOutdated)
{
    const auto updateResult = TslService::triggerTslUpdateIfNecessary(
        *mRequestSender,
        *mXmlValidator,
        *mTslTrustStore,
        onlyOutdated);
    Expect(mTslTrustStore->hasTsl() && !mTslTrustStore->isTslTooOld(),
           "Can not load actual TSL file, unknown unexpected error");

    mBnaTrustStore->setUpdateUrls(mTslTrustStore->getBnaUrls());
    TslService::triggerTslUpdateIfNecessary(
        *mRequestSender,
        *mXmlValidator,
        *mBnaTrustStore,
        onlyOutdated,
        mTslTrustStore->getBnaSignerCertificates());
    Expect(mBnaTrustStore->hasTsl() && !mBnaTrustStore->isTslTooOld(),
            "Can not load actual BNetzA-VL file, unknown unexpected error");

    mBnaTrustStore->setBnaOcspMapping(mTslTrustStore->getBnaOcspMapping());

    if (updateResult == TslService::UpdateResult::Updated)
    {
        notifyPostUpdateHooks();
    }

    return updateResult;
}


void TslManager::notifyPostUpdateHooks()
{
    std::lock_guard lock(mUpdateHookMutex);

    for (const auto& postUpdateHook : mPostUpdateHooks)
    {
        if (postUpdateHook != nullptr)
            postUpdateHook();
    }
}
