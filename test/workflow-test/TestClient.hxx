#ifndef ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX

#include "erp/client/response/ClientResponse.hxx"

#include <functional>
#include <memory>

class ClientRequest;

/// @brief Implements backend for workflow tests
///
/// Workflow tests can be executed as part of:
/// * erp-test (EndpointTestClient)
/// * erp-integration-test (HttpsTestClient)
///
/// This class basically wraps the call to HttpsClient.send and directs it to
/// desired server
class TestClient
{
public:
    using Factory = std::function<std::unique_ptr<TestClient>(void)>;

    static void setFactory(Factory&& factory);
    static std::unique_ptr<TestClient> create();

    TestClient() = default;
    virtual ~TestClient() = default;

    virtual ClientResponse send(const ClientRequest& clientRequest) = 0;

    TestClient(const TestClient&) = delete;
    TestClient(TestClient&&) = delete;
    TestClient& operator = (const TestClient&) = delete;
    TestClient& operator = (TestClient&&) = delete;

    virtual std::string getHostHttpHeader() const = 0;
    virtual std::string getHostAddress() const = 0;
    virtual uint16_t getPort() const = 0;

private:
    static Factory mFactory;
};


#endif // ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX
