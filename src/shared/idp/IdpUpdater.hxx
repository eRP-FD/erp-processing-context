/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_IDPUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_IDPUPDATER_HXX

#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/util/PeriodicTimer.hxx"
#include "shared/util/UrlHelper.hxx"
#include "shared/ErpRequirements.hxx"

class Idp;
class TslManager;

/**
 * Update the IDP signer certificate every hour (https://dth01.ibmgcloud.net/jira/browse/ERP-4645).
 * If the update fails write a log message that allows an observer to trigger a priority 2 incidence after multiple failed
 * update attempts.
 * Update failure count is (will be) persisted in Redis
 *
 * If the update is successful, verify that the certificate is valid.
 *
 * The update is triggered every hour and when the TSL is updated.
 *
 * This class has virtual functions to allow tests to simulate error cases.
 */
class IdpUpdater
{
public:
    /**
     * Create a new instance of the IdpUpdater.
     *
     * @param certificateHolder
     *        An updated and validated IDP signer certificate is set at this Idp object.
     * @param tslManager
     *        Used to verify downloaded IDP signer certificates.
     * @param runFirstUpdateSynchronously
     *        When true, then the first and synchronous call to update() is made before the constructor returns.
     * @param urlRequestSender
     *        The request sender to use for download of the IDP-Certificate
     */
    template<class IdpUpdaterType = IdpUpdater>
    static std::unique_ptr<IdpUpdaterType> create (Idp& certificateHolder,
               TslManager& tslManager,
               boost::asio::io_context& context,
               bool runFirstUpdateSynchronously = true,
               const std::shared_ptr<UrlRequestSender>& urlRequestSender = {});
    IdpUpdater (Idp& certificateHolder,
               TslManager& tslManager,
               const std::shared_ptr<UrlRequestSender>& urlRequestSender,
               boost::asio::io_context& context);
    virtual ~IdpUpdater (void);

    void update (void);

protected:
    virtual std::string doDownloadWellknown (void);
    virtual UrlHelper::UrlParts doParseWellknown (std::string&& wellKnownConfiguration);
    virtual std::string doDownloadDiscovery (const UrlHelper::UrlParts& url);
    virtual std::vector<Certificate> doParseDiscovery (std::string&& jwkString);

    virtual void doVerifyCertificate (const std::vector<Certificate>& certificateChain);

    enum class UpdateStatus
    {
        Success,
        WellknownDownloadFailed,
        DiscoveryDownloadFailed,
        VerificationFailed,
        UnknownFailure
    };
    virtual void reportUpdateStatus (UpdateStatus status, std::string_view details);

    void setCertificateMaxAge (std::chrono::system_clock::duration maxAge);

private:
    void startUpdateTimer();
    std::chrono::system_clock::duration updateInterval() const;

    size_t mUpdateFailureCount{0};
    Idp& mCertificateHolder;
    TslManager& mTslManager;
    std::shared_ptr<UrlRequestSender> mRequestSender;
    std::unique_ptr<UrlHelper::UrlParts> mUpdateUrl;
    std::atomic_bool mIsUpdateActive;
    std::optional<size_t> mUpdateHookId;
    std::chrono::system_clock::duration mCertificateMaxAge;
    std::chrono::system_clock::time_point mLastSuccessfulUpdate;
    int updateIntervalMinutes;
    int noCertificateUpdateIntervalSeconds;
    std::chrono::milliseconds mResolveTimeout;

    class RefreshTimer;
    friend class RefreshTimer;
    std::unique_ptr<PeriodicTimerBase> mUpdater;
    boost::asio::io_context& mIo;

    Certificate getUpToDateCertificate (void);
    UrlHelper::UrlParts downloadAndParseWellknown (void);
    std::vector<Certificate> downloadAndParseDiscovery (const UrlHelper::UrlParts& url);
    void verifyCertificate (const std::vector<Certificate>& certificateChain);

    std::string getMessageText (const UpdateStatus status) const;
};


template<class IdpUpdaterType>
std::unique_ptr<IdpUpdaterType> IdpUpdater::create (
    Idp& certificateHolder,
    TslManager& tslManager,
    boost::asio::io_context& ioContext,
    bool runFirstUpdateSynchronously,
    const std::shared_ptr<UrlRequestSender>& urlRequestSender)
{
    auto idpUpdater = std::make_unique<IdpUpdaterType>(certificateHolder, tslManager, urlRequestSender, ioContext);

    A_20974.start("start first update and verify of the IDP synchronously on application start");
    if (runFirstUpdateSynchronously)
        idpUpdater->update();
    A_20974.finish();

    return idpUpdater;
}


#endif
