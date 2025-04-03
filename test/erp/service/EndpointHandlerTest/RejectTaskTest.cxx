/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Consent.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/service/task/RejectTaskHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/util/ByteHelper.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

class RejectTaskTest : public EndpointHandlerTest
{
protected:
    JWT jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim).value();

    const std::string validAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    const std::string prescriptionId =
       model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714).toString();

    void dispenseTask(std::string secret)
    {
        const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                        .telematikId = telematikId};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);

        Header requestHeader{HttpMethod::POST,
                             "/Task/" + prescriptionId + "/$dispense/",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId});
        serverRequest.setAccessToken(std::move(jwtPharmacy));
        serverRequest.setQueryParameters({{"secret", secret}});
        serverRequest.setBody(std::move(medicationDispenseXml));

        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        DispenseTaskHandler handler({});
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    }

    std::string acceptTask()
    {
        std::string secret;
        [&]()
        {
            AcceptTaskHandler handler({});
            const Header requestHeader{HttpMethod::POST, "/Task/" + prescriptionId + "/$accept/", 0, {}, HttpStatus::Unknown};
            ServerRequest serverRequest{Header(requestHeader)};
            serverRequest.setPathParameters({"id"}, {prescriptionId});
            serverRequest.setQueryParameters({{"ac", validAccessCode}});
            serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
            ServerResponse serverResponse;
            AccessLog accessLog;
            SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
            ASSERT_NO_THROW(handler.handleRequest(sessionContext));
            auto view = Fhir::instance()
                            .structureRepository(model::Timestamp::now())
                            .match(&Fhir::instance().backend(),
                                   ResourceTemplates::Versions::latest(
                                       "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task"));
            std::optional<model::Bundle> bundle;
            ASSERT_NO_THROW(bundle = model::ResourceFactory<model::Bundle>::fromXml(serverResponse.getBody(),
                                                                                    *StaticData::getXmlValidator())
                                         .getValidated(model::ProfileType::fhir, view));
            ASSERT_TRUE(bundle.has_value());
            auto acceptedTasks = bundle->getResourcesByType<model::Task>();
            ASSERT_EQ(acceptedTasks.size(), 1);
            EXPECT_TRUE(acceptedTasks[0].owner().has_value());
            ASSERT_TRUE(acceptedTasks[0].secret().has_value());
            secret = *acceptedTasks[0].secret();
        }();
        return secret;
    }

    void rejectTask(std::string secret)
    {
        RejectTaskHandler rejectTaskHandler({});
        const Header requestHeader{HttpMethod::POST, "/Task/" + prescriptionId + "/$reject/", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId});
        serverRequest.setQueryParameters({{"secret", secret}});
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke());
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(rejectTaskHandler.handleRequest(sessionContext));
    }

    void testTask(const std::function<void(model::Task&)>& additionalTest = [](model::Task&){})
    {
        GetTaskHandler getTaskHandler({});
        const Header requestHeader{HttpMethod::GET, "/Task/" + prescriptionId, 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{Header(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId});
        serverRequest.setQueryParameters({{"ac", validAccessCode}});
        serverRequest.setAccessToken(
            JwtBuilder::testBuilder().makeJwtVersicherter(std::string(ResourceTemplates::TaskOptions{}.kvnr)));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(getTaskHandler.handleRequest(sessionContext));
        std::optional<model::UnspecifiedResource> unspec;
        ASSERT_NO_THROW(unspec.emplace(model::UnspecifiedResource::fromXmlNoValidation(serverResponse.getBody())));
        ASSERT_NO_THROW(testutils::bestEffortValidate(*unspec));
        auto bundle = model::Bundle::fromJson(std::move(unspec).value().jsonDocument());
        auto tasks = bundle.getResourcesByType<model::Task>();
        ASSERT_EQ(tasks.size(), 1);
        ASSERT_FALSE(tasks[0].secret().has_value());
        ASSERT_NO_THROW(additionalTest(tasks[0]));
    }
};

TEST_F(RejectTaskTest, OwnerDeleted)
{
    std::string secret = acceptTask();

    rejectTask(secret);

    testTask([](model::Task& savedTask)
    {
        // GEMREQ-start A_24175#test1
        A_24175.test("task.owner has been deleted");
        EXPECT_FALSE(savedTask.owner().has_value());
        // GEMREQ-end A_24175#test1
    });
}

TEST_F(RejectTaskTest, MedicationDispenseDeleted)
{
    using namespace ResourceTemplates;
    if(Versions::GEM_ERP_current() != Versions::GEM_ERP_1_3)
    {
        GTEST_SKIP();
    }

    std::string secret = acceptTask();

    dispenseTask(secret);

    testTask([](model::Task& savedTask)
    {
        EXPECT_TRUE(savedTask.lastMedicationDispense().has_value());
    });

    rejectTask(secret);

    testTask([](model::Task& savedTask)
    {
        // GEMREQ-start A_24286-02#test1
        A_24286_02.test("task.lastMedicationDispense has been deleted");
        EXPECT_FALSE(savedTask.lastMedicationDispense().has_value());
        // GEMREQ-end A_24286-02#test1
    });

}
