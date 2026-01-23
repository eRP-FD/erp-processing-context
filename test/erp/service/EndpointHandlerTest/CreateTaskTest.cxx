#include "erp/service/task/CreateTaskHandler.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/TestUtils.hxx"

class CreateTaskTest : public EndpointHandlerTest
{
};

TEST_F(CreateTaskTest, multiView)
{
    const auto now{model::Timestamp::now()};
    testutils::ShiftFhirResourceViewsGuard shiftView("2024-11-01", floor<std::chrono::days>(now.toChronoTimePoint()));
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);

    std::string body = R"(
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

TEST_F(CreateTaskTest, multiViewERP26615)
{
    const auto now{model::Timestamp::now()};
    const auto today = date::floor<date::days>(now.toChronoTimePoint());
    const auto gematik13End = date::year_month_day{date::year{2025}, date::month{4}, date::day{16}};
    const auto offset = today - date::sys_days{gematik13End};
    testutils::ShiftFhirResourceViewsGuard shiftViews(offset);
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);

    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <meta>
      <profile value="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input|1.4"/>
   </meta>
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

TEST_F(CreateTaskTest, multiViewERP26615_2)
{
    const auto now{model::Timestamp::now()};
    const auto today = date::floor<date::days>(now.toChronoTimePoint());
    const auto gematik13End = date::year_month_day{date::year{2025}, date::month{4}, date::day{16}};
    const auto offset = today - date::sys_days{gematik13End};
    testutils::ShiftFhirResourceViewsGuard shiftViews(offset);
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);

    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <meta>
      <profile value="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input"/>
   </meta>
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

TEST_F(CreateTaskTest, multiViewERP26615_3)
{
    const auto now{model::Timestamp::now()};
    const auto today = date::floor<date::days>(now.toChronoTimePoint());
    const auto gematik13End = date::year_month_day{date::year{2025}, date::month{4}, date::day{16}};
    const auto offset = today - date::sys_days{gematik13End};
    testutils::ShiftFhirResourceViewsGuard shiftViews(offset);
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);

    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <meta>
      <profile value="https://gematik.de/fhir/erp/StructureDefinition/Mumpiz"/>
   </meta>
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
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error",
                                          "invalid profile https://gematik.de/fhir/erp/StructureDefinition/Mumpiz must "
                                          "be one of: http://hl7.org/fhir/StructureDefinition/Parameters|4.0.1");
}

TEST_F(CreateTaskTest, invalidERP27183)
{
    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="abc"/>
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
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "error getting workFlowType from parameters", "stoi");
}

TEST_F(CreateTaskTest, UnslicedExtensionReject)
{
    A_22927_02.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard envGuard{
        "ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM", model::Timestamp::now().toGermanDate()};

    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <meta>
      <extension url="subStatus"><valueBoolean value="true"/></extension>
   </meta>
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
    std::string diagnostics = "Parameters.meta.extension[0]: error: element doesn't belong to any slice. "
                              "(from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); "
                              "Parameters.meta.extension[0]: error: element doesn't belong to any slice. "
                              "(from profile: http://hl7.org/fhir/StructureDefinition/Meta|4.0.1); ";
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(
        handler.handleRequest(sessionContext), "Unintendierte Verwendung von Extensions an unspezifizierter Stelle",
        diagnostics);
}

TEST_F(CreateTaskTest, UnslicedExtensionAllow)
{
    A_22927_02.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard envGuard{"ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM",
                                      (model::Timestamp::now() + std::chrono::days{1}).toGermanDate()};

    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <meta>
      <extension url="subStatus"><valueBoolean value="true"/></extension>
   </meta>
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
    EXPECT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
}


