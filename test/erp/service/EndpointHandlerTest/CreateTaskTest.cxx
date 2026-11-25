#include "erp/service/task/CreateTaskHandler.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/TestUtils.hxx"

static constexpr char bodyFmt[] = R"(
<Parameters xmlns="http://hl7.org/fhir">
{}   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="160"/>
      </valueCoding>
   </parameter>
</Parameters>
)";

static constexpr char metaProfileFmt[] = R"(
   <meta>
      <profile value="{}"/>
   </meta>
)";


class CreateTaskTest : public EndpointHandlerTest
{
};

struct CreateTaskMultiviewTestParam {
    std::string name;
    std::optional<std::string> profile;
    bool expectSuccess = true;
};

class CreateTaskMultiviewTest : public CreateTaskTest, public testing::WithParamInterface<CreateTaskMultiviewTestParam>
{
public:
    std::string body(const std::optional<std::string>& profile)
    {
        std::string metaProfile;
        if (profile)
        {
            metaProfile = fmt::format(metaProfileFmt, *profile);
        }
        return fmt::format(bodyFmt, metaProfile);
    }
};

TEST_P(CreateTaskMultiviewTest, multiView)
{
    const auto now{model::Timestamp::now()};
    testutils::ShiftFhirResourceViewsGuard shiftView("GEM_WF_1_6_NON_FDV", floor<std::chrono::days>(now.toChronoTimePoint()));
    auto views = Fhir::instance().structureRepository(now);
    ASSERT_GE(views.size(), 2);
    CreateTaskHandler handler({});
    ServerRequest request(Header(HttpMethod::POST, "/Task/$create", 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown));
    request.setBody(body(GetParam().profile));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    handler.preHandleRequestHook(sessionContext);
    if (GetParam().expectSuccess)
    {
        handler.handleRequest(sessionContext);
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
    }
    else
    {
        auto diag = fmt::format("invalid profile {} must "
                                "be one of: http://hl7.org/fhir/StructureDefinition/Parameters|4.0.1",
                                value(GetParam().profile));
        EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                              "parsing / validation error", diag);
    }
}

INSTANTIATE_TEST_SUITE_P(valid, CreateTaskMultiviewTest,
                         ::testing::ValuesIn<std::list<CreateTaskMultiviewTestParam>>({
                             {"no_profile", std::nullopt},
                             {"base_profile_no_ver", "http://hl7.org/fhir/StructureDefinition/Parameters"},
                             {"base_profile_with_ver", "http://hl7.org/fhir/StructureDefinition/Parameters|4.0.1"},
                         }));

INSTANTIATE_TEST_SUITE_P(
    invalid, CreateTaskMultiviewTest,
    ::testing::ValuesIn<std::list<CreateTaskMultiviewTestParam>>({
        {
            "removed_profile_no_version",
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input",
            false,
        },
        {
            "removed_profile_old_ver",
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input|1.4",
            false,
        },
        {
            "removed_profile_current_ver1",
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input|1.5",
            false,
        },
        {
            "removed_profile_current_ver2",
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input|1.6",
            false,
        },
        {
            "unknown_profile_no_ver",
            "https://gematik.de/fhir/erp/StructureDefinition/Mumpiz",
            false,
        },
        {
            "unknown_profile_with_ver",
            "https://gematik.de/fhir/erp/StructureDefinition/Mumpiz|1.5",
            false,
        },
    }));

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
    A_22927_03.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
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
    A_22927_03.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
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

TEST_F(CreateTaskTest, TRezeptFeatureTrue)
{
    EnvironmentVariableGuard envGuard{"ERP_FEATURE_TREZEPT", "true"};
    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="166"/>
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

TEST_F(CreateTaskTest, TRezeptFeatureFalse)
{
    EnvironmentVariableGuard envGuard{"ERP_FEATURE_TREZEPT", "false"};
    std::string body = R"(
<Parameters xmlns="http://hl7.org/fhir">
   <parameter>
      <name value="workflowType"/>
      <valueCoding>
         <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType"/>
         <code value="166"/>
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
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                      "Erstellung von Tasks mit flowtype 166 noch nicht zulässig");
}
