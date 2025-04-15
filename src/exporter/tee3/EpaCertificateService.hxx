/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#ifndef EPA_MEDICATION_EXPORTER_EPACERTIFICATESERVICE_HXX
#define EPA_MEDICATION_EXPORTER_EPACERTIFICATESERVICE_HXX

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/network/client/TlsCertificateVerifier.hxx"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <map>
#include <optional>
#include <thread>

class Certificate;

/// @brief retrieves the Certificate required for Tee3-Protocol
/// call use_service\<EpaCertificateService\>(iocontext) to use it.
/// To avoid deadlocks, it uses an own worker thread with an io context
/// to download the certificate, if required.
class EpaCertificateService : public boost::asio::execution_context::service
{
public:
    using key_type = EpaCertificateService;
    struct CertId {
        std::string hash;
        uint64_t version;
        //NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
        auto operator<=>(const CertId&) const = default;
    };

    EpaCertificateService(boost::asio::execution_context& context);
    ~EpaCertificateService() override;

    Certificate provideCertificate(const std::string& hostname, uint16_t port, CertId certId);
    void setTlsCertificateVerifier(TlsCertificateVerifier tlsCertificateVerifier);
    bool hasTlsCertificateVerifier();

    static boost::asio::execution_context::id id;

protected:
    void shutdown() override;

private:
    // returns immediately when the certificate is in cache, otherwise
    // will request the certificate id from the given endpoint
    boost::asio::awaitable<shared_X509> provideCertificateInternal(std::string hostname, uint16_t port,
                                                                   CertId certId);

    boost::asio::io_context mWorkIoContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> mWork;
    std::thread mWorkThread;
    std::map<CertId, Certificate> mCertificates;
    std::optional<TlsCertificateVerifier> mTlsCertificateVerifier;
};

std::ostream& operator<<(std::ostream&, const EpaCertificateService::CertId&);
std::string to_string(const EpaCertificateService::CertId&);

#endif// EPA_MEDICATION_EXPORTER_EPACERTIFICATESERVICE_HXX
