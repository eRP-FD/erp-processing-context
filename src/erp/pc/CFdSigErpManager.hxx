#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_PC_CFDSIGERPMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_PC_CFDSIGERPMANAGER_HXX

#include "shared/crypto/Certificate.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/deprecated/TimerJobBase.hxx"
#include "shared/deprecated/Timer.hxx"

#include <mutex>

class Configuration;
class HsmPool;
enum class CertificateType;
class ErpBlob;
class OcspResponse;
class TslManager;

/**
 * This class allows to manage C.FD.SIG certificate of eRP processing context
 * including required validations.
 */
class CFdSigErpManager : public TimerJobBase
{
public:
    CFdSigErpManager(
        const Configuration& configuration,
        TslManager& tslManager,
        HsmPool& hsmPool);

    ~CFdSigErpManager() override;

    /**
     * Returns C.FD.SIG eRP certificate if the last validation was successful,
     * otherwise TslException is thrown.
     */
    [[nodiscard]] Certificate getCertificate();

    [[nodiscard]] shared_EVP_PKEY getPrivateKey();

    /**
     * If no tsl manager is provided (a special case for test scenarios), then an empty optional is returned.
     *
     * If a tsl manager is provided, it returns a non-empty optional with a valid ( according to configured OCSP-Grace-Period )
     * OCSP-Response related data for C.FD.SIG eRP certificate if the last validation was successful,
     * or throws TslError in case of problems.
     *
     * @param forceOcspRequest if set to true the OCSP-request is forced
     */
    OcspResponse getOcspResponseData(const bool forceOcspRequest);

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
     * Returns string representation of last successful OCSP response timestamp or "never succeeded" if
     * it was never successfully validated.
     */
    std::string getLastOcspResponseTimestamp();

    [[nodiscard]] CertificateType getCertificateType() const;

    [[nodiscard]] std::string getCertificateNotAfterTimestamp() const;

    void updateOcspResponseCacheOnBlobCacheUpdate();

protected:
    void onStart(void) override;
    void executeJob(void) override;
    void onFinish(void) override;

private:
    OcspResponse internalGetOcspResponseData(const Certificate& certificate, const bool forceOcspRequest);

    TslManager& mTslManager;
    std::optional<size_t> mValidationHookId;
    HsmPool& mHsmPool;

    std::shared_mutex mLastProducedAtMutex;
    std::chrono::system_clock::time_point mLastProducedAt;

    std::chrono::system_clock::duration mOcspRequestGracePeriod;

    // private key cache
    shared_EVP_PKEY mCFdSigErpKey;
    ErpBlob mPkBlob;
    std::mutex mMutexPkCache;
};


#endif
