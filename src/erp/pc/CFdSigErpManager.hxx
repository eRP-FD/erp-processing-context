#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_PC_CFDSIGERPMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_PC_CFDSIGERPMANAGER_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/util/TimerJobBase.hxx"
#include "erp/util/Timer.hxx"

#include <mutex>


/**
 * This class allows to manage C.FD.SIG certificate of eRP processing context
 * including required validations.
 */
class CFdSigErpManager : public TimerJobBase
{
public:
    CFdSigErpManager(
        const Configuration& configuration,
        std::shared_ptr<TslManager> tslManager);

    ~CFdSigErpManager();

    /**
     * Returns C.FD.SIG eRP certificate if the last validation was successful,
     * otherwise TslException is thrown.
     */
    const Certificate& getCertificate();

    /**
     * If no tsl manager is provided (a special case for test scenarios), then an empty optional is returned.
     *
     * If a tsl manager is provided, it returns a non-empty optional with a valid ( according to configured OCSP-Grace-Period )
     * OCSP-Response related data for C.FD.SIG eRP certificate if the last validation was successful,
     * or throws TslError in case of problems.
     *
     * @param forceOcspRequest if set to true the OCSP-request is forced
     */
    std::optional<TrustStore::OcspResponseData> getOcspResponseData(const bool forceOcspRequest);

    /**
     * If no tsl manager is provided (a special case for test scenarios), then nullptr is returned.
     *
     * If a tsl manager is provided, returns a not-null pointer to a valid ( according to configured OCSP-Grace-Period )
     * OCSP-Response OpenSsl object for C.FD.SIG eRP certificate if the last validation was successful,
     * or throws TslError in case of problems.
     */
    OcspResponsePtr getOcspResponse();

    /**
     * Checks whether the C.FD.SIG eRP certificate is successfully validated and there is a cached
     * OCSP-Response that is still valid according to OCSP-Grace-Period.
     * Throws std::runtime_exception if there is no valid cached OCSP-Response.
     */
    void healthCheck();

    /**
     * Returns string representation of last successful validation timestamp or "never succeeded" if
     * it was never successfully validated.
     */
    std::string getLastValidationTimestamp();

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    std::optional<TrustStore::OcspResponseData> internalGetOcspResponseData(const bool forceOcspRequest);

    std::mutex mMutex;
    const Certificate mCFdSigErp;
    X509Certificate mX509Certificate;

    const std::shared_ptr<TslManager> mTslManager;
    std::optional<size_t> mValidationHookId;

    std::chrono::system_clock::duration mOcspRequestGracePeriod;
    std::chrono::system_clock::time_point mLastSuccess;
    bool mLastCheckSuccessfull;
};


#endif
