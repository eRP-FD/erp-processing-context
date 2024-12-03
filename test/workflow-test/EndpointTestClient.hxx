/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEXT_ENDPOINTTESTCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TEXT_ENDPOINTTESTCLIENT_HXX

#include "shared/network/client/HttpsClient.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/workflow-test/TestClient.hxx"
#include "test/util/StaticData.hxx"

class EndpointTestClient final : public TestClient
{
public:
    static constexpr char mLocalHost[] = "127.0.0.1";

    static std::unique_ptr<TestClient> factory(std::shared_ptr<XmlValidator> xmlValidator, Target target);

    EndpointTestClient(std::shared_ptr<XmlValidator> xmlValidator, Target target);

    ClientResponse send(const ClientRequest& clientRequest) override;

    std::string getHostHttpHeader() const override;
    std::string getHostAddress() const override;
    uint16_t getPort() const override;
    ~EndpointTestClient() override;

    PcServiceContext* getContext() const override;

    bool runsInErpTest() const override;

private:
    Database::Factory createDatabaseFactory();
    static std::unique_ptr<HttpsServer> httpsServerFactory(const std::string_view address, uint16_t port,
                                                           RequestHandlerManager&& requestHandlers,
                                                           BaseServiceContext& serviceContext,
                                                           bool enforceClientAuthentication,
                                                           const SafeString& caCertificates);


    bool isPostgresEnabled();
    void initAdminServer();
    void initEnrolmentServer();
    void initVauServer(std::shared_ptr<XmlValidator> xmlValidator);
    void initClient();
    void initVsdmKeys();

    std::unique_ptr<MockDatabase> mMockDatabase;
    std::unique_ptr<HttpsServer> mServer;
    std::unique_ptr<HttpsClient> mHttpsClient;
    std::unique_ptr<PcServiceContext> mContext;
    std::unique_ptr<HsmPool> mPool;
    const Target mTarget;
    std::unique_ptr<EnvironmentVariableGuard> mPortGuard;
};


#endif // ERP_PROCESSING_CONTEXT_TEXT_ENDPOINTTESTCLIENT_HXX
