#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"

#include "shared/model/Resource.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/util/TestUtils.hxx"

class CreateTaskTest : public EndpointHandlerTest {


};

TEST_F(CreateTaskTest, multiView)
{
    const auto now{model::Timestamp::now()};
    testutils::ShiftFhirResourceViewsGuard shiftView("2024-11-01", floor<std::chrono::days>(now.toChronoTimePoint()));
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);

    std::string body =R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";

    CreateTaskHandler handler({});
    ServerRequest request(Header(HttpMethod::POST, "/Task/$create", 0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown));
    request.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    handler.preHandleRequestHook(sessionContext);
    handler.handleRequest(sessionContext);
    EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
}
