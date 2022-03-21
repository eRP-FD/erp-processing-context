/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockOcspServer.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockOcsp.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/mock/UrlRequestSenderMock.hxx"
#include "test/util/StaticData.hxx"

#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/ErpRequestHandler.hxx"

#include <test_config.h>


using OcspSessionContext = SessionContext<OcspServiceContext>;

namespace
{
    Certificate createOcspCertificate()
    {
        const std::string ocspCertificatePem =
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
        return Certificate::fromPemString(ocspCertificatePem);
    }

    shared_EVP_PKEY createOcspPrivateKey()
    {
        std::string ocspPrivateKeyPem =
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem");
        return EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{std::move(ocspPrivateKeyPem)});
    }

    class OCSPStatusRequestHandler : public UnconstrainedRequestHandler<OcspServiceContext>
    {
    public:
        OCSPStatusRequestHandler(const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs_)
            : UnconstrainedRequestHandler<OcspServiceContext>()
            , ocspResponderKnownCertificateCaPairs(ocspResponderKnownCertificateCaPairs_)
            , ocspCertificate(createOcspCertificate())
            , ocspPrivateKey(createOcspPrivateKey())
        {
        }

        virtual void handleRequest (OcspSessionContext& session) override
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

        virtual Operation getOperation (void) const override
        {
            return Operation::UNKNOWN;
        }

        std::vector<MockOcsp::CertificatePair> ocspResponderKnownCertificateCaPairs;
        Certificate ocspCertificate;
        shared_EVP_PKEY ocspPrivateKey;
    };
}


std::unique_ptr<HttpsServer<OcspServiceContext>> MockOcspServer::create(
    const std::string& hostIp,
    uint16_t port,
    const std::vector<MockOcsp::CertificatePair> ocspResponderKnownCertificateCaPairs)
{
    RequestHandlerManager<OcspServiceContext> handlerManager;
    std::unique_ptr<RequestHandlerInterface<OcspServiceContext>> requestHandler =
        std::make_unique<OCSPStatusRequestHandler>(ocspResponderKnownCertificateCaPairs);
    handlerManager.onPostDo("/test_path/I_OCSP_Status_Information", std::move(requestHandler));

    std::unique_ptr<OcspServiceContext> serviceContext = std::make_unique<OcspServiceContext>();

    return std::make_unique<HttpsServer<OcspServiceContext>>(
        hostIp,
        port,
        std::move(handlerManager),
        std::move(serviceContext));
}


// Instantiate server templates for the MockOcspServer and its OcspServiceContext.

#include "erp/server/HttpsServer.ixx"
#include "erp/server/ServerSocketHandler.ixx"
#include "erp/server/context/SessionContext.ixx"
#include "erp/server/handler/RequestHandlerContext.ixx"
#include "erp/server/handler/RequestHandlerManager.ixx"
#include "erp/server/session/ServerSession.ixx"
#include <erp/server/response/ServerResponse.hxx>
#include <erp/server/response/ServerResponse.hxx>
#include <erp/server/response/ServerResponse.hxx>


template class HttpsServer<OcspServiceContext>;
template class RequestHandlerContainer<OcspServiceContext>;
template class RequestHandlerContext<OcspServiceContext>;
template class RequestHandlerManager<OcspServiceContext>;
template class ServerSocketHandler<OcspServiceContext>;
template class SessionContext<OcspServiceContext>;

template std::shared_ptr<ServerSession> ServerSession::createShared (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& context,
    const RequestHandlerManager<OcspServiceContext>& requestHandlers,
    OcspServiceContext& serviceContext);
