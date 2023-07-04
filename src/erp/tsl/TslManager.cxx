/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tsl/TslManager.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/tsl/X509Store.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"


namespace
{
    std::vector<std::string> getInitialTslDownloadUrls()
    {
        return {Configuration::instance().getOptionalStringValue(
            ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL,
            "http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml")};
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
}


#define HANDLE_EXCEPTION(prefix) \
    handleException(std::current_exception(), prefix, fileAndLine);


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
                                   X509Certificate& certificate,
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
TrustStore::OcspResponseData TslManager::getCertificateOcspResponse(
    const TslMode tslMode,
    X509Certificate& certificate,
    const std::unordered_set<CertificateType>& typeRestrictions,
    const OcspCheckDescriptor& ocspCheckDescriptor)
{
    // no mutex is necessary because trust store is thread safe
    try
    {
        std::optional<TrustStore::OcspResponseData> ocspResponse;

        GS_A_4898.start("Minimize TSL-Grace-Period, check TSL-validity and try to update if invalid each time a certificate is validated.");
        const TslService::UpdateResult updateResult = internalUpdate(true);
        GS_A_4898.finish();

        if (updateResult == TslService::UpdateResult::NotUpdated
            && ocspCheckDescriptor.mode == OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE
            && ocspCheckDescriptor.providedOcspResponse == nullptr)
        {
            // the cached OCSP-Response still can be used, if it exists and is still valid
            ocspResponse = getTrustStore(tslMode).getCachedOcspData(certificate.getSha256FingerprintHex());
        }

        if ( ! ocspResponse.has_value())
        {
            TslService::checkCertificate(
                certificate,
                typeRestrictions,
                *mRequestSender,
                getTrustStore(tslMode),
                ocspCheckDescriptor);

            // The certificate check was successful, so the cache must have now OCSP response according to the
            // mode from `ocspCheckDescriptor.mode` specified for the check.
            ocspResponse = getTrustStore(tslMode).getCachedOcspData(certificate.getSha256FingerprintHex());

            TslExpect4(ocspResponse.has_value(),
                       "OCSP response - can not parse provided in cache OCSP response",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       tslMode);
            ocspResponse->fromCache = false;
        }
        else
        {
            ocspResponse->fromCache = true;
        }

        OcspService::checkOcspStatus(ocspResponse->status, std::nullopt, getTrustStore(tslMode));

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
