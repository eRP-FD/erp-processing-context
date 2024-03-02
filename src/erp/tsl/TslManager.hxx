/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_TSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_TSLMANAGER_HXX

#include "erp/client/UrlRequestSender.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/tsl/OcspCheckDescriptor.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/TslMode.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/util/Configuration.hxx"

class X509Store;

class TslManager
{
public:
    using TslManagerFactory = std::function<std::shared_ptr<TslManager>(const std::shared_ptr<XmlValidator>& xmlValidator)>;
    using PostUpdateHook = std::function<void(void)>;

    /**
     * Creates the manager from configuration:
     * - loads the initial tsl file and validates it
     * - download the new version if the initial file is outdated and validates it
     *
     * @param requestSender     sender to send requests for TSL-update and OCSP-checks
     * @param initialPostUpdateHooks hooks that should be used by the manager from the beginning
     *
     * @throws TslError in case of problems
     */
    TslManager(
        std::shared_ptr<UrlRequestSender> requestSender,
        std::shared_ptr<XmlValidator> xmlValidator,
        const std::vector<PostUpdateHook>& initialPostUpdateHooks = {});

    virtual ~TslManager() = default;

    /**
     * This implementation checks whether the current tsl information is up to date,
     * downloads the new version of tsl file if necessary and validates it,
     * and returns the filled trusted certificate store for the specified certificate.
     *
     * @param tslMode           specifies which trust store should be provided TSL or BNetzA-VL
     * @param certificate       if provided, specifies for which certificate the trusted certificate store should be created
     *
     * @return a reference to the filled trusted certificate store.
     *
     * @throws TslError in case of problems
     */
    virtual X509Store getTslTrustedCertificateStore(
        const TslMode tslMode,
        const std::optional<X509Certificate>& certificate);

    /**
     * This implementation checks whether the current tsl information is up to date,
     * downloads the new version of tsl file if necessary and validates it,
     * and does verification including OCSP-Validation of the provided certificate.
     *
     * @param tslMode                   specifies which trust store should be provided TSL or BNetzA-VL
     * @param certificate               the certificate to check
     * @param typeRestrictions          if provided, the certificate type must be in the set
     * @param ocspCheckDescriptor       specifies how the OCSP check should be done
     *
     * @throws TslError in case of problems
     */
    virtual void verifyCertificate(const TslMode tslMode,
                                   X509Certificate& certificate,
                                   const std::unordered_set<CertificateType>& typeRestrictions,
                                   const OcspCheckDescriptor& ocspCheckDescriptor);

    /**
     * Allows to get OCSP-response for the provided certificate according to specified OCSP check descriptor.
     *
     * @param tslMode                   specifies which trust store should be provided TSL or BNetzA-VL
     * @param certificate               the certificate to get OCSP-Response for
     * @param typeRestrictions          if provided, the certificate type must be in the set
     * @param ocspCheckDescriptor       specifies how the OCSP check should be done
     *
     * @throws TslError in case of problems
     */
    virtual TrustStore::OcspResponseData getCertificateOcspResponse(
        const TslMode tslMode,
        X509Certificate& certificate,
        const std::unordered_set<CertificateType>& typeRestrictions,
        const OcspCheckDescriptor& ocspCheckDescriptor);

    /**
     * Checks whether the trust stores have to be update and does the update if necessary.
     */
    virtual void updateTrustStoresOnDemand();

    /**
     * Adds a callback that is called after a successful TSL update.
     * The order of called callbacks is not guarantied.
     *
     * The callbacks are not expected to throw any exception, except the cases when
     * further execution of the application makes no sense.
     * In case an exception is thrown, it is handled as an unknown exception by TSL update
     * and might lead to application termination after the TSL update success is required.
     *
     * @returns the id of the hook that can be used to remove it
     *
     * @throws std::runtime_error in case of problems
     */
    size_t addPostUpdateHook (const PostUpdateHook& postUpdateHook);

    /**
     * Disables the registered callback with specified index.
     * It is not necessary to explicitly disable the callback,
     * the destruction of the TslManager will clean the list of callbacks anyway.
     *
     * This method is intended to simplify the objects destruction at the end of application runtime,
     * the components can then disable own hooks to make the destruction more safe.
     */
    void disablePostUpdateHook(const size_t hookId);

    virtual TrustStore::HealthData healthCheckTsl() const;
    virtual TrustStore::HealthData healthCheckBna() const;

protected:
    /**
     *  The constructor is only intended for testing purposes.
     */
    TslManager(
        std::shared_ptr<UrlRequestSender> requestSender,
        std::shared_ptr<XmlValidator> xmlValidator,
        const std::vector<PostUpdateHook>& initialPostUpdateHooks,
        std::unique_ptr<TrustStore> tslTrustStore,
        std::unique_ptr<TrustStore> bnaTrustStore);

private:
    TrustStore& getTrustStore(const TslMode tslMode);
    TslService::UpdateResult internalUpdate(const bool onlyOutdated);
    void notifyPostUpdateHooks();

    const std::shared_ptr<UrlRequestSender> mRequestSender;
    const std::shared_ptr<XmlValidator> mXmlValidator;

    std::unique_ptr<TrustStore> mTslTrustStore;
    std::unique_ptr<TrustStore> mBnaTrustStore;

    std::mutex mUpdateHookMutex; // Named mutex because at the moment it is used only to guard the mPostUpdateHook.
    std::vector<PostUpdateHook> mPostUpdateHooks;

#ifdef FRIEND_TEST

    friend class TslTestHelper;

#endif

#if WITH_HSM_MOCK > 0
    friend class MockTslManager;
#endif
};


#endif
