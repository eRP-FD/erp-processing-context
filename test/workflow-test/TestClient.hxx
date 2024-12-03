/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX

#include "shared/network/client/response/ClientResponse.hxx"

#include <functional>
#include <iosfwd>
#include <memory>

class ClientRequest;
class Certificate;
class XmlValidator;
class PcServiceContext;

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
    enum class Target {
        ADMIN,
        ENROLMENT,
        VAU,
    };


    using Factory = std::function<std::unique_ptr<TestClient>(std::shared_ptr<XmlValidator>, Target target)>;

    static void setFactory(Factory&& factory);
    /// @brief create a new TestClient
    /// @param xmlValidator xmlValidator instance to use (usualy StaticData::getXmlValidator())
    ///                     can be nullptr when @p target == ADMIN
    /// @param target target to test
    static std::unique_ptr<TestClient> create(std::shared_ptr<XmlValidator> xmlValidator, Target target = Target::VAU);

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

    virtual Certificate getEciesCertificate();

    virtual PcServiceContext* getContext() const;

    virtual bool runsInErpTest() const = 0;


private:
    static Factory mFactory;
};
std::string to_string(TestClient::Target target);
std::ostream& operator << (std::ostream& out, TestClient::Target target);

#endif // ERP_PROCESSING_CONTEXT_TEST_TESTCLIENT_HXX
