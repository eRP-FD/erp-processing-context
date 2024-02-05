#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"

#include "EndpointHandlerTest.hxx"
#include "erp/util/Demangle.hxx"
#include "test/util/StaticData.hxx"

class ChargeItemDeleteEndpointTest : public EndpointHandlerTest {

};

TEST_F(ChargeItemDeleteEndpointTest, DeleteNonPkvFails)
{
    ChargeItemDeleteHandler handler{{}};
    auto serviceContext = StaticData::makePcServiceContext();
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    Header requestHeader{HttpMethod::PATCH, "/chargeitem/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext(serviceContext, serverRequest, serverResponse, accessLog);
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try
    {
        handler.handleRequest(sessionContext);
        ADD_FAILURE() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
        EXPECT_STREQ(ex.what(), "Attempt to access charge item for non-PKV Prescription.");
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }

}
