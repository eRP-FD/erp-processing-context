#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

#include "erp/model/Parameters.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/service/task/AbortTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/RejectTaskHandler.hxx"
#include "erp/service/task/TaskHandler.hxx"
#include "erp/util/Configuration.hxx"

#include "test/util/EnvironmentVariableGuard.hxx"

class FeatureToggleWF200Test : public EndpointHandlerTest, public ::testing::WithParamInterface<bool>
{
public:
    FeatureToggleWF200Test()
        : mPkvFeatureEnvVar{ConfigurationKey::FEATURE_PKV, "false"}
    {

    }
    void SetUp() override
    {
        mWf200FeatureEnvVar.emplace(ConfigurationKey::FEATURE_WORKFLOW_200, GetParam()?"true":"false");
    }
protected:
    model::PrescriptionId id = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 83475634827);

    template <typename HandlerType>
    void check(SessionContext&);
private:
    EnvironmentVariableGuard mPkvFeatureEnvVar;
    std::optional<EnvironmentVariableGuard> mWf200FeatureEnvVar;

};

template<typename HandlerType>
void FeatureToggleWF200Test::check(SessionContext& sessionContext)//NOLINT(readability-function-cognitive-complexity)
{
    HandlerType handler{{}};
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    if (GetParam()) // WF200 enabled ?
    {
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
    }
    else
    {
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);

    }
}

TEST_P(FeatureToggleWF200Test, task_create)//NOLINT(readability-function-cognitive-complexity)
{
    CreateTaskHandler handler({});

    std::string body = R"-(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="workflowType"/>
        <valueCoding>
            <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
            <code value="200"/>
            <display value="PKV (Apothekenpflichtige Arzneimittel)"/>
        </valueCoding>
    </parameter>
</Parameters>)-";

;

    ServerRequest serverRequest{{HttpMethod::POST, "/Task/$create", Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtArzt());
    serverRequest.setBody(std::move(body));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    if (GetParam()) // WF200 enabled ?
    {
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    }
    else
    {
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);

    }
}


TEST_P(FeatureToggleWF200Test, task_activate)
{
    ServerRequest serverRequest{{HttpMethod::POST, "/Task/" + id.toString() + "/$activate", Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtArzt());
    serverRequest.setPathParameters({"id"},{id.toString()});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<ActivateTaskHandler>(sessionContext));
}


TEST_P(FeatureToggleWF200Test, task_accept)
{
    std::string accessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    auto path = "/Task/" + id.toString() + "/$accept?ac=" + accessCode;
    ServerRequest serverRequest{{HttpMethod::POST, std::move(path), Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtApotheke());
    serverRequest.setPathParameters({"id"},{id.toString()});
    serverRequest.setQueryParameters({{"ac", accessCode}});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<AcceptTaskHandler>(sessionContext));
}


TEST_P(FeatureToggleWF200Test, task_reject)
{
    std::string secret = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    auto path = "/Task/" + id.toString() + "/$reject?secret=" + secret;
    ServerRequest serverRequest{{HttpMethod::POST, std::move(path), Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtApotheke());
    serverRequest.setPathParameters({"id"},{id.toString()});
    serverRequest.setQueryParameters({{"secret", secret}});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<RejectTaskHandler>(sessionContext));
}

TEST_P(FeatureToggleWF200Test, task_close)
{
    std::string secret = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    auto path = "/Task/" + id.toString() + "/$close?secret=" + secret;
    ServerRequest serverRequest{{HttpMethod::POST, std::move(path), Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtApotheke());
    serverRequest.setPathParameters({"id"},{id.toString()});
    serverRequest.setQueryParameters({{"secret", secret}});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<CloseTaskHandler>(sessionContext));
}

TEST_P(FeatureToggleWF200Test, task_abort)
{
    ServerRequest serverRequest{{HttpMethod::POST, "/Task/" + id.toString() + "/$abort", Header::Version_1_1,
                                {{Header::ContentType, ContentMimeType::fhirXmlUtf8}}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({"id"},{id.toString()});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<AbortTaskHandler>(sessionContext));
}

TEST_P(FeatureToggleWF200Test, get_task)
{
    ServerRequest serverRequest{{HttpMethod::GET, "/Task/" + id.toString(), Header::Version_1_1, {},
                                 HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({"id"},{id.toString()});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(check<GetTaskHandler>(sessionContext));
}


TEST_P(FeatureToggleWF200Test, medicationDispense_get)
{
    ServerRequest serverRequest{{HttpMethod::GET, "/MedicationDispense/" + id.toString(), Header::Version_1_1, {},
                                 HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setPathParameters({"id"},{id.toString()});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    ASSERT_NO_FATAL_FAILURE(check<GetMedicationDispenseHandler>(sessionContext));
}


TEST_P(FeatureToggleWF200Test, medicationDispense_getAll)//NOLINT(readability-function-cognitive-complexity)
{
    static constexpr auto* prefix = "https://gematik.de/fhir/NamingSystem/PrescriptionID|";

    GetAllMedicationDispenseHandler handler{{}};

    ServerRequest serverRequest{{HttpMethod::GET, "/MedicationDispense", Header::Version_1_1, {}, HttpStatus::Unknown}};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    serverRequest.setQueryParameters({{"identifier", prefix + id.toString()}});

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    if (GetParam()) // WF200 enabled ?
    {
        EXPECT_NO_THROW(handler.handleRequest(sessionContext));
    }
    else
    {
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);

    }
}



INSTANTIATE_TEST_SUITE_P(toggle, FeatureToggleWF200Test, ::testing::Values(true, false),
                        [](auto info){ return info.param?"enabled":"disabled";});
