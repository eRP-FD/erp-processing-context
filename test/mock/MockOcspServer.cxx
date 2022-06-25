/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockOcspServer.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockDatabase.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/FileHelper.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/StaticData.hxx"

#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/ErpRequestHandler.hxx"

#include <test_config.h>


using OcspSessionContext = SessionContext;

namespace
{
    Certificate createOcspCertificate()
    {
        const std::string ocspCertificatePem =
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
        return Certificate::fromPem(ocspCertificatePem);
    }

    shared_EVP_PKEY createOcspPrivateKey()
    {
        std::string ocspPrivateKeyPem =
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem");
        return EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{std::move(ocspPrivateKeyPem)});
    }

    class OCSPStatusRequestHandler : public UnconstrainedRequestHandler
    {
    public:
        OCSPStatusRequestHandler(const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs_)
            : UnconstrainedRequestHandler()
            , ocspResponderKnownCertificateCaPairs(ocspResponderKnownCertificateCaPairs_)
            , ocspCertificate(createOcspCertificate())
            , ocspPrivateKey(createOcspPrivateKey())
        {
        }

        void handleRequest (OcspSessionContext& session) override
        {
            std::string responseBody =
                MockOcsp::create(
                    session.request.getBody(),
                    ocspResponderKnownCertificateCaPairs,
                    ocspCertificate,
                    ocspPrivateKey).toDer();

            session.response.setStatus(HttpStatus::OK);
            session.response.setBody(responseBody);
        }

        Operation getOperation (void) const override
        {
            return Operation::UNKNOWN;
        }

        std::vector<MockOcsp::CertificatePair> ocspResponderKnownCertificateCaPairs;
        Certificate ocspCertificate;
        shared_EVP_PKEY ocspPrivateKey;
    };
}


std::unique_ptr<HttpsServer> MockOcspServer::create(
    const std::string& hostIp,
    uint16_t port,
    const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs)
{
    RequestHandlerManager handlerManager;
    std::unique_ptr<RequestHandlerInterface> requestHandler =
        std::make_unique<OCSPStatusRequestHandler>(ocspResponderKnownCertificateCaPairs);
    handlerManager.onPostDo("/test_path/I_OCSP_Status_Information", std::move(requestHandler));

    static PcServiceContext mContext = StaticData::makePcServiceContext();

    return std::make_unique<HttpsServer>(
        hostIp,
        port,
        std::move(handlerManager),
        mContext);
}
