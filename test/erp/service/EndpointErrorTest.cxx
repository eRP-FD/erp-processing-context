/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ServerTestBase.hxx"

#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequest.hxx"


namespace {
    class ErrorHandler : public RequestHandlerInterface<PcServiceContext>
    {
    public:
        void handleRequest (SessionContext<PcServiceContext>& session) override
        {
            const auto requestedResponseStatus = std::stoi(session.request.getPathParameter("status").value_or("200"));
            throw ErpException("test", fromBoostBeastStatus(requestedResponseStatus));
        }
        [[nodiscard]] bool allowedForProfessionOID(std::string_view) const override
        {
            return true;
        }
        Operation getOperation (void) const override
        {
            return mOperation;
        }

        void setOperation (const Operation operation)
        {
            mOperation = operation;
        }

    private:
        Operation mOperation = Operation::UNKNOWN;
    };
}


/**
 * Simulate errors while processing a request by throwing an ErpException with a HttpStatus code that is given in the
 * request URL.
 */
class EndpointErrorTest : public ServerTestBase
{
public:
    void SetUp (void) override
    {
        ServerTestBase::SetUp();
        setMockedOperation(Operation::UNKNOWN);
    }

    void setMockedOperation (const Operation operation)
    {
        ASSERT_NE(mRequestHandler, nullptr);
        auto* errorHandler = dynamic_cast<ErrorHandler*>(mRequestHandler->handler.get());
        ASSERT_NE(errorHandler, nullptr);
        errorHandler->setOperation(operation);
    }
protected:
    void addAdditionalSecondaryHandlers (RequestHandlerManager<PcServiceContext>& requestManager) override
    {
        mRequestHandler = &requestManager.addRequestHandler(
            HttpMethod::POST,
            "/error/{status}",
            std::make_unique<ErrorHandler>());
    }

    void testStatusCode (uint32_t statusCode, uint32_t expectedResponseStatus, bool expectResponseBody);

private:
    RequestHandlerContext<PcServiceContext>* mRequestHandler = nullptr;
};


TEST_F(EndpointErrorTest, testStatusCodeHandling_operationGetTask)
{
    const std::vector<std::pair<uint32_t,uint32_t>> knownStatusCodes = {
        // Progress or Success status codes (100-103 and 200-226) are not handled via exception and are therefore not
        // be covered by this test.
        {300, 308},
        {400, 408}, /** 409 is not supported for this operation */ {410, 451},
        {500, 511}};

    const std::set<uint32_t> supportedStatusCodes = {
        200, 201, 204,
        400, 401, 403, 404, 406, 408, 409, 410, 429};

    // Make requests where the handler will throw an ErpException with the status code that
    // is given in the request path.
    // Depending on whether this status code is supported, i.e. listed in Table 1 of requirement A_19514
    // we expect as response status code
    // - the status code in the request path if it is in the Gematik list
    // - 500 (Internal Server Error) in all other cases.
    setMockedOperation(Operation::GET_Task_id);
    for (const auto& range : knownStatusCodes)
    {
        for (uint32_t status=range.first; status<=range.second; ++status)
        {
            bool expectResponseBody = status == 500 || supportedStatusCodes.count(status);
            testStatusCode(status, expectResponseBody ? status : 500, expectResponseBody);
        }
    }
}


TEST_F(EndpointErrorTest, testStatusCodeHandling_operationPostTaskIdAccept)
{
    const std::vector<uint32_t> statusCodes = {
        409
    };

    const std::set<uint32_t> supportedStatusCodes = {
        200, 201, 204,
        400, 401, 403, 404, 408, 409, 410, 429,
        500};

    setMockedOperation(Operation::POST_Task_id_accept);
    for (const auto& status : statusCodes)
    {
        testStatusCode(status, status, true/*expectResponseBody*/);
    }
}


void EndpointErrorTest::testStatusCode (const uint32_t statusCode,
                                        const uint32_t expectedResponseStatus,
                                        bool expectResponseBody)
{
    auto client = createClient();

    // Create the inner request
    ClientRequest request(
        createPostHeader("/error/" + std::to_string(statusCode)),
        "");

    // Send the request.
    auto outerResponse = client.send(encryptRequest(request));

    // Verify and decrypt the outer response.
    auto innerResponse = verifyOuterResponse(outerResponse);

    // Verify that the status code has been correctly returned.
    // Use a hard cast instead of fromBoostBeastStatus to also test this function as well.
    ASSERT_EQ(innerResponse.getHeader().status(), static_cast<HttpStatus>(expectedResponseStatus))
        << "got status " << static_cast<uint32_t>(innerResponse.getHeader().status()) << " but expected "
        << expectedResponseStatus;
    if(expectResponseBody)
    {
        ASSERT_NE(innerResponse.getBody(), ""); // => error response exists
    }
}
