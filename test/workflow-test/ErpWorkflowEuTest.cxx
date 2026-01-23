// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessCode.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/database/PostgresConnection.hxx"
#include "shared/model/GemErpEuPrOrganization.hxx"
#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <pqxx/transaction>

struct ErpWorkflowEuTestParams {
    model::PrescriptionType workflowType;
    bool featureToggle;
    bool expectedSuccess;
};
std::ostream& operator<<(std::ostream& os, const ErpWorkflowEuTestParams& params)
{
    return os << (int) magic_enum::enum_integer(params.workflowType) << "_" << std::boolalpha << params.featureToggle
              << "_" << std::boolalpha << params.expectedSuccess;
}

class ErpWorkflowEuTestP : public ::testing::TestWithParam<ErpWorkflowEuTestParams>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    void SetUp() override
    {
        if (! testBase.runsInErpTest() && ! GetParam().featureToggle)
        {
            // Feature toggle is enabled in DEV-2
            GTEST_SKIP_("Feature toggle cannot be changed remotely.");
        }
        if (testBase.runsInErpTest())
        {
            auto db = testBase.client->getContext()->databaseFactory();
            if (auto* mockDb = dynamic_cast<MockDatabaseProxy*>(&db->getBackend()))
            {
                mockDb->addCountryCode("FR");
            }
            else
            {
                db.reset();
                auto connection = std::make_unique<pqxx::connection>(PostgresConnection::defaultConnectString());
                auto txn = pqxx::work{*connection};
                txn.exec("DELETE FROM erp.eu_country_codes");
                txn.exec("INSERT INTO erp.eu_country_codes (iso_3166_alpha_2) VALUES ('FR')");
                txn.commit();
            }
        }
        // else: in DEV testdata is added to the database with resources/test/sql scripts.
    }
    void testPostConsent()
    {
        const auto startTime = model::Timestamp::now();
        auto expectedStatus = GetParam().featureToggle ? HttpStatus::Created : HttpStatus::MethodNotAllowed;
        auto expectedErrorCode = GetParam().featureToggle ? std::make_optional<model::OperationOutcome::Issue::Type>()
                                                          : model::OperationOutcome::Issue::Type::not_supported;
        std::optional<model::Consent> consent;
        ASSERT_NO_FATAL_FAILURE(consent =
                                    testBase.consentPost(model::ConsentType::EUDISPCONS, kvnr.id(),
                                                         model::Timestamp::now(), expectedStatus, expectedErrorCode));
        if (GetParam().featureToggle)
        {
            ASSERT_TRUE(consent.has_value());
            testBase.checkAuditEvents({"EUDISPCONS-" + kvnr.id()}, kvnr.id(), "de", startTime, {kvnr.id()}, {},
                                      {model::AuditEvent::SubType::create});
        }
        else
        {
            ASSERT_FALSE(consent.has_value());
            testBase.checkAuditEvents(std::vector<std::optional<std::string>>{}, kvnr.id(), "de", startTime, {}, {},
                                      {});
        }
    }

    void postChargcons()
    {
        ASSERT_NO_FATAL_FAILURE(
            testBase.consentPost(model::ConsentType::CHARGCONS, kvnr.id(), model::Timestamp::now()));
    }

    std::optional<model::GemErpEuPrParAccessAuthorizationResponse>
    postEuAccessPermission(const ResourceTemplates::EuAccessPermissionOptions& options, HttpStatus expectedStatus)
    {
        std::optional<model::GemErpEuPrParAccessAuthorizationResponse> euAccessPermission;
        postEuAccessPermissionInternal(euAccessPermission, options, expectedStatus);
        return euAccessPermission;
    }
    void postEuAccessPermissionInternal(
        std::optional<model::GemErpEuPrParAccessAuthorizationResponse>& outEuAccessPermission,
        const ResourceTemplates::EuAccessPermissionOptions& options, HttpStatus expectedStatus)
    {
        auto json = ResourceTemplates::euAccessPermissionRequestJson(options);

        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
            args.overrideExpectedEuWorkflowVersion = "XXX";
        }
        else
        {
            args.expectedBdeUseCase = bde::GrantEuAuthorization_UC_3_16;
        }

        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                testBase.send(ErpWorkflowTestBase::RequestArguments{args}
                                  .withHttpMethod(HttpMethod::POST)
                                  .withVauPath("/$grant-eu-access-permission")
                                  .withBody(json)
                                  .withContentType(ContentMimeType::fhirJsonUtf8)
                                  .withJwt(jwt)
                                  .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                                  .withExpectedInnerStatus(expectedStatus)));

        if (expectedStatus == HttpStatus::Created)
        {
            ASSERT_NO_THROW(outEuAccessPermission = model::GemErpEuPrParAccessAuthorizationResponse::fromJson(
                                serverResponse.getBody(), *testBase.getJsonValidator()));
        }
    }

    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> readEuAccessPermission(HttpStatus expectedStatus)
    {
        std::optional<model::GemErpEuPrParAccessAuthorizationResponse> euAccessPermission;
        readEuAccessPermissionInternal(euAccessPermission, expectedStatus);
        return euAccessPermission;
    }
    void readEuAccessPermissionInternal(
        std::optional<model::GemErpEuPrParAccessAuthorizationResponse>& outEuAccessPermission,
        HttpStatus expectedStatus)
    {
        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
        }
        else
        {
            args.expectedBdeUseCase = bde::ReadEuAuthorization_UC_3_18;
        }
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                testBase.send(ErpWorkflowTestBase::RequestArguments{args}
                                  .withHttpMethod(HttpMethod::GET)
                                  .withVauPath("/$read-eu-access-permission")
                                  .withJwt(jwt)
                                  .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                                  .withExpectedInnerStatus(expectedStatus)));

        if (expectedStatus == HttpStatus::OK)
        {
            ASSERT_NO_THROW(outEuAccessPermission = model::GemErpEuPrParAccessAuthorizationResponse::fromJson(
                                serverResponse.getBody(), *testBase.getJsonValidator()));
        }
    }

    void revokeEuAccessPermission(HttpStatus expectedStatus)
    {
        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
        }
        else
        {
            args.expectedBdeUseCase = bde::RevokeEuAuthorization_UC_3_17;
        }

        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                testBase.send(ErpWorkflowTestBase::RequestArguments{args}
                                  .withHttpMethod(HttpMethod::DELETE)
                                  .withVauPath("/$revoke-eu-access-permission")
                                  .withJwt(jwt)
                                  .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                                  .withExpectedInnerStatus(expectedStatus)));
    }

    std::optional<model::Task> patchTask(const ResourceTemplates::EuPatchTaskOptions& options,
                                         const model::PrescriptionId& prescriptionId, HttpStatus expectedStatus)
    {
        std::optional<model::Task> task;
        patchTaskInternal(task, options, prescriptionId, expectedStatus);
        return task;
    }
    void patchTaskInternal(std::optional<model::Task>& outTask, const ResourceTemplates::EuPatchTaskOptions& options,
                           const model::PrescriptionId& prescriptionId, HttpStatus expectedStatus)
    {
        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
        }

        auto json = ResourceTemplates::euPatchTaskJson(options);
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) =
                testBase.send(ErpWorkflowTestBase::RequestArguments{args}
                                  .withHttpMethod(HttpMethod::PATCH)
                                  .withVauPath("/Task/" + prescriptionId.toString())
                                  .withBody(json)
                                  .withJwt(jwt)
                                  .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                                  .withHeader(Header::ContentType, ContentMimeType::fhirJsonUtf8)
                                  .withExpectedInnerStatus(expectedStatus)
                                  .withExpectedBdeUseCase(bde::PatchTask_UC_PATCH_TASK)));

        EXPECT_FALSE(serverResponse.getBody().empty());
        ASSERT_NO_FATAL_FAILURE(outTask = model::Task::fromJsonNoValidation(serverResponse.getBody()));
    }

    [[nodiscard]]
    std::unique_ptr<model::FhirResourceBase>
    postGetEuPrescriptions(const ResourceTemplates::EuPostGetPrescriptionsOptions& options)
    {
        std::unique_ptr<model::FhirResourceBase> result;
        postGetEuPrescriptionsInternal(options, result);
        return result;
    }
    void postGetEuPrescriptionsInternal(const ResourceTemplates::EuPostGetPrescriptionsOptions& options,
                                        std::unique_ptr<model::FhirResourceBase>& result)
    {
        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtNcpeh();
        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
        }
        if (options.requestType == model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics)
        {
            args.queryParams.emplace("_count", "1");
        }
        auto getUseCase = [&] {
            switch (options.requestType)
            {
                case model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics:
                    return bde::GetDemographics_UC_4_19;
                case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list:
                    return bde::GetPrescriptionsList_UC_4_20;
                case model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval:
                    return bde::GetPrescriptionsRetrieval_UC_4_21;
            }
            return bde::GetDemographics_UC_4_19;
        };

        auto xml = ResourceTemplates::euPostGetPrescriptionsXml(options);
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) = testBase.send(
                ErpWorkflowTestBase::RequestArguments{args}
                    .withHttpMethod(HttpMethod::POST)
                    .withVauPath("/$get-eu-prescriptions")
                    .withBody(xml)
                    .withJwt(jwt)
                    .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                    .withHeader(Header::ContentType, ContentMimeType::fhirXml)
                    .withExpectedInnerStatus(GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::NotFound)
                    .withExpectedBdeUseCase(getUseCase())));

        EXPECT_FALSE(serverResponse.getBody().empty());
        ASSERT_NO_FATAL_FAILURE(result = testutils::createResource(serverResponse.getBody()));
        ASSERT_NE(result, nullptr);
    }

    void postEuClose(const ResourceTemplates::EuCloseTaskOptions& options)
    {
        ClientResponse serverResponse;
        ClientResponse outerResponse;
        JWT jwt = JwtBuilder::testBuilder().makeJwtNcpeh();
        ErpWorkflowTestBase::RequestArguments args{};
        if (! GetParam().featureToggle)
        {
            args.overrideExpectedInnerOperation = "UNKNOWN";
        }
        auto xml = ResourceTemplates::euCloseTaskXml(options);
        ASSERT_NO_FATAL_FAILURE(
            std::tie(std::ignore, serverResponse) = testBase.send(
                ErpWorkflowTestBase::RequestArguments{args}
                    .withHttpMethod(HttpMethod::POST)
                    .withVauPath("/Task/" + options.prescriptionId + "/$eu-close")
                    .withBody(xml)
                    .withJwt(jwt)
                    .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                    .withHeader(Header::ContentType, ContentMimeType::fhirXml)
                    .withExpectedInnerStatus(GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::BadRequest)
                    .withExpectedBdeUseCase(bde::EuClose_UC_4_22)));
    }

    EnvironmentVariableGuard featureToggleGuard{"ERP_FEATURE_EU", GetParam().featureToggle ? "true" : "false"};
    ErpWorkflowTestBase testBase;

    model::Kvnr kvnr = testBase.generateNewRandomKVNR();
    testutils::ShiftFhirResourceViewsGuard shift{"EU_2025_10_01",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
    model::EuAccessCode accessCode{SafeString{"aaaaaa"}};
};

TEST_P(ErpWorkflowEuTestP, PostConsent)
{
    testPostConsent();
    if (GetParam().expectedSuccess)
    {
        A_22162_01.test("Only single consent for the same Kvnr");
        ASSERT_NO_FATAL_FAILURE(testBase.consentPost(model::ConsentType::EUDISPCONS, kvnr.id(), model::Timestamp::now(),
                                                     HttpStatus::Conflict,
                                                     model::OperationOutcome::Issue::Type::conflict));
    }
}

TEST_P(ErpWorkflowEuTestP, PostConsentBoth)
{
    testPostConsent();
    postChargcons();
}


TEST_P(ErpWorkflowEuTestP, GetConsent)
{
    std::vector<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_TRUE(consent.empty());

    testPostConsent();
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_EQ(consent.empty(), ! GetParam().expectedSuccess);
    if (! consent.empty())
    {
        EXPECT_EQ(consent[0].consentCategory(), model::ConsentType::EUDISPCONS);
    }
}

TEST_P(ErpWorkflowEuTestP, GetConsentBoth)
{
    std::vector<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(testPostConsent());
    ASSERT_NO_FATAL_FAILURE(postChargcons());

    size_t expectedSize = GetParam().expectedSuccess ? 2 : 1;
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_EQ(consent.size(), expectedSize);
    if (expectedSize == 2)
    {
        EXPECT_TRUE(std::ranges::any_of(consent, [](const auto& c) {
            return c.consentCategory() == model::ConsentType::EUDISPCONS;
        }));
        EXPECT_TRUE(std::ranges::any_of(consent, [](const auto& c) {
            return c.consentCategory() == model::ConsentType::CHARGCONS;
        }));
    }
    else
    {
        EXPECT_EQ(consent[0].consentCategory(), model::ConsentType::CHARGCONS);
    }
}

TEST_P(ErpWorkflowEuTestP, GetConsentBothFiltered)
{
    std::vector<model::Consent> consent;
    testPostConsent();
    postChargcons();

    size_t expectedSize = GetParam().expectedSuccess ? 1 : 0;
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id(), model::ConsentType::EUDISPCONS));
    ASSERT_EQ(consent.size(), expectedSize);
    if (expectedSize == 1)
    {
        EXPECT_EQ(consent[0].consentCategory(), model::ConsentType::EUDISPCONS);
    }

    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id(), model::ConsentType::CHARGCONS));
    ASSERT_EQ(consent.size(), 1);
    EXPECT_EQ(consent[0].consentCategory(), model::ConsentType::CHARGCONS);
}

TEST_P(ErpWorkflowEuTestP, DeleteConsent)
{
    const auto startTime = model::Timestamp::now();
    A_22158.test("Consent löschen - Löschen der Consent");
    auto expectedStatus = GetParam().expectedSuccess ? HttpStatus::NotFound : HttpStatus::MethodNotAllowed;
    std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode =
        GetParam().expectedSuccess ? model::OperationOutcome::Issue::Type::not_found
                                   : model::OperationOutcome::Issue::Type::not_supported;
    ASSERT_NO_FATAL_FAILURE(
        testBase.consentDelete(model::ConsentType::EUDISPCONS, kvnr.id(), expectedStatus, expectedErrorCode));

    testPostConsent();

    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NoContent : HttpStatus::MethodNotAllowed;
    expectedErrorCode = GetParam().expectedSuccess ? std::make_optional<model::OperationOutcome::Issue::Type>()
                                                   : model::OperationOutcome::Issue::Type::not_supported;
    ASSERT_NO_FATAL_FAILURE(
        testBase.consentDelete(model::ConsentType::EUDISPCONS, kvnr.id(), expectedStatus, expectedErrorCode));
    std::vector<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_TRUE(consent.empty());

    if (GetParam().featureToggle)
    {
        testBase.checkAuditEvents({"EUDISPCONS-" + kvnr.id()}, kvnr.id(), "de", startTime, {kvnr.id(), kvnr.id()}, {},
                                  {model::AuditEvent::SubType::create, model::AuditEvent::SubType::del});
    }
    else
    {
        testBase.checkAuditEvents(std::vector<std::optional<std::string>>{}, kvnr.id(), "de", startTime, {}, {}, {});
    }
}

TEST_P(ErpWorkflowEuTestP, DeleteConsentBoth)
{
    const auto startTime = model::Timestamp::now();
    testPostConsent();
    postChargcons();

    std::vector<model::Consent> consent;
    auto expectedStatus = GetParam().expectedSuccess ? HttpStatus::NoContent : HttpStatus::MethodNotAllowed;
    auto expectedErrorCode = GetParam().expectedSuccess ? std::make_optional<model::OperationOutcome::Issue::Type>()
                                                        : model::OperationOutcome::Issue::Type::not_supported;
    ASSERT_NO_FATAL_FAILURE(
        testBase.consentDelete(model::ConsentType::EUDISPCONS, kvnr.id(), expectedStatus, expectedErrorCode));
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_EQ(consent.size(), 1);
    EXPECT_EQ(consent[0].consentCategory(), model::ConsentType::CHARGCONS);
    ASSERT_NO_FATAL_FAILURE(testBase.consentDelete(model::ConsentType::CHARGCONS, kvnr.id()));
    ASSERT_NO_FATAL_FAILURE(consent = testBase.consentGet(kvnr.id()));
    ASSERT_TRUE(consent.empty());
    if (GetParam().featureToggle)
    {
        testBase.checkAuditEvents(
            {"EUDISPCONS-" + kvnr.id(), "CHARGCONS-" + kvnr.id(), "EUDISPCONS-" + kvnr.id(), "CHARGCONS-" + kvnr.id()},
            kvnr.id(), "de", startTime, {kvnr.id(), kvnr.id(), kvnr.id(), kvnr.id()}, {},
            {model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::del,
             model::AuditEvent::SubType::del});
    }
    else
    {
        testBase.checkAuditEvents({"CHARGCONS-" + kvnr.id(), "CHARGCONS-" + kvnr.id()}, kvnr.id(), "de", startTime,
                                  {kvnr.id(), kvnr.id()}, {},
                                  {model::AuditEvent::SubType::create, model::AuditEvent::SubType::del});
    }
}

TEST_P(ErpWorkflowEuTestP, EuAccessPermission)
{
    ASSERT_NO_FATAL_FAILURE(testPostConsent());

    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto startTime = model::Timestamp::now();
    // retrieve -- NotFound
    HttpStatus expectedStatus = GetParam().expectedSuccess ? HttpStatus::NotFound : HttpStatus::MethodNotAllowed;
    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> euAccessPermission;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission = readEuAccessPermission(expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), false);

    // revoke -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NoContent : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(revokeEuAccessPermission(expectedStatus));

    // grant -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::Created : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission =
                                postEuAccessPermission(ResourceTemplates::EuAccessPermissionOptions{}, expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    A_27092.test("The previous access permission is overwritten");
    // grant -- duplicate OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::Created : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission =
                                postEuAccessPermission(ResourceTemplates::EuAccessPermissionOptions{}, expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    // retrieve -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission = readEuAccessPermission(expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    // revoke -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NoContent : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(revokeEuAccessPermission(expectedStatus));

    // retrieve -- NotFound
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NotFound : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission = readEuAccessPermission(expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), false);

    // grant -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::Created : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission =
                                postEuAccessPermission(ResourceTemplates::EuAccessPermissionOptions{}, expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    // retrieve -- OK
    expectedStatus = GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission = readEuAccessPermission(expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    if (GetParam().featureToggle)
    {
        testBase.checkAuditEvents({"eu-access-permission-" + kvnr.id() + "-FR"}, kvnr.id(), "de", startTime,
                                  {kvnr.id(), kvnr.id(), kvnr.id(), kvnr.id()}, {},
                                  {model::AuditEvent::SubType::create, model::AuditEvent::SubType::create,
                                   model::AuditEvent::SubType::del, model::AuditEvent::SubType::create});
    }
    else
    {
        testBase.checkAuditEvents(std::vector<std::optional<std::string>>{}, kvnr.id(), "de", startTime, {}, {}, {});
    }
}

TEST_P(ErpWorkflowEuTestP, EuAccessPermissionRemovedOnDeleteConsent)
{
    A_27131.test("Consent löschen - EUDISPCONS - Löschen Zugriffsberechtigung");
    testPostConsent();
    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> euAccessPermission;
    auto expectedStatus = GetParam().expectedSuccess ? HttpStatus::Created : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission =
                                postEuAccessPermission(ResourceTemplates::EuAccessPermissionOptions{}, expectedStatus));
    ASSERT_EQ(euAccessPermission.has_value(), GetParam().expectedSuccess);

    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NoContent : HttpStatus::MethodNotAllowed;
    auto expectedErrorCode = GetParam().expectedSuccess ? std::make_optional<model::OperationOutcome::Issue::Type>()
                                                        : model::OperationOutcome::Issue::Type::not_supported;
    ASSERT_NO_FATAL_FAILURE(
        testBase.consentDelete(model::ConsentType::EUDISPCONS, kvnr.id(), expectedStatus, expectedErrorCode));

    expectedStatus = GetParam().expectedSuccess ? HttpStatus::NotFound : HttpStatus::MethodNotAllowed;
    ASSERT_NO_FATAL_FAILURE(euAccessPermission = readEuAccessPermission(expectedStatus));
    ASSERT_FALSE(euAccessPermission.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    Workflow_featureToggle_expectedSuccess, ErpWorkflowEuTestP,
    testing::Values(ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichigeArzneimittel, false, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichigeArzneimittel, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, false, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, true, true}));

class ErpWorkflowEuTestPatchTaskP : public ErpWorkflowEuTestP
{
};

TEST_P(ErpWorkflowEuTestPatchTaskP, PatchTask)
{
    const auto startTime = model::Timestamp::now();
    testPostConsent();
    auto accessPermissionResonse = postEuAccessPermission({"FR", accessCode.toString().c_str()}, HttpStatus::Created);

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = testBase.taskCreate(GetParam().workflowType));
    ASSERT_TRUE(task.has_value());
    testBase.taskActivate(
        task->prescriptionId(), task->accessCode(),
        std::get<0>(testBase.makeQESBundle(kvnr.id(), task->prescriptionId(), model::Timestamp::now())));

    const ResourceTemplates::EuPatchTaskOptions options{.version = ResourceTemplates::Versions::GEM_ERPEU_current(),
                                                        .EuRedeemableFlag = true};
    std::optional<model::Task> patchedTask;
    ASSERT_NO_FATAL_FAILURE(patchedTask =
                                patchTask(options, task->prescriptionId(),
                                          GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::Forbidden));
    ASSERT_TRUE(patchedTask.has_value());
    if (GetParam().expectedSuccess)
    {
        task.emplace(std::move(*patchedTask));
    }
    const auto telematikId = testBase.jwtArzt().stringForClaim(JWT::idNumberClaim);
    if (GetParam().expectedSuccess)
    {
        ASSERT_TRUE(task.has_value());
        EXPECT_TRUE(task->isEuRedeemableByPatientAuthorization());
        EXPECT_TRUE(task->isEuRedeemableByProperties());
        testBase.checkAuditEvents({"EUDISPCONS-" + kvnr.id(), "eu-access-permission-" + kvnr.id() + "-FR",
                                   task->prescriptionId().toString(), task->prescriptionId().toString()},
                                  kvnr.id(), "de", startTime, {kvnr.id(), kvnr.id(), *telematikId, kvnr.id()}, {2u},
                                  {model::AuditEvent::SubType::create, model::AuditEvent::SubType::create,
                                   model::AuditEvent::SubType::update, model::AuditEvent::SubType::update});
    }
    else
    {
        // the same first three, but the fourth is missing:
        testBase.checkAuditEvents(
            {"EUDISPCONS-" + kvnr.id(), "eu-access-permission-" + kvnr.id() + "-FR", task->prescriptionId().toString()},
            kvnr.id(), "de", startTime, {kvnr.id(), kvnr.id(), *telematikId}, {2u},
            {model::AuditEvent::SubType::create, model::AuditEvent::SubType::create,
             model::AuditEvent::SubType::update});
    }
}

INSTANTIATE_TEST_SUITE_P(
    PatchTask, ErpWorkflowEuTestPatchTaskP,
    testing::Values(ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichigeArzneimittel, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::digitaleGesundheitsanwendungen, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisung, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisungPkv, true, false}));


class ErpWorkflowEuTestGetPrescriptionsP : public ErpWorkflowEuTestP
{
public:
    void SetUp() override
    {
        ErpWorkflowEuTestP::SetUp();
        testPostConsent();
        auto accessPermissionResonse =
            postEuAccessPermission({"FR", accessCode.toString().c_str()}, HttpStatus::Created);
        prepareTask(0);
        prepareTask(1);
        prepareTask(2);
    }

    void prepareTask(size_t idx)
    {
        std::optional<model::Task> task;
        ASSERT_NO_FATAL_FAILURE(task = testBase.taskCreate(GetParam().workflowType));
        ASSERT_TRUE(task.has_value());
        testBase.taskActivate(task->prescriptionId(), task->accessCode(),
                              std::get<0>(testBase.makeQESBundle(kvnr.id(), task->prescriptionId(),
                                                                 model::Timestamp::now() + std::chrono::minutes(idx))));

        const ResourceTemplates::EuPatchTaskOptions options{.version = ResourceTemplates::Versions::GEM_ERPEU_current(),
                                                            .EuRedeemableFlag = true};
        std::optional<model::Task> patchedTask;
        ASSERT_NO_FATAL_FAILURE(patchedTask =
                                    patchTask(options, task->prescriptionId(),
                                              GetParam().expectedSuccess ? HttpStatus::OK : HttpStatus::Forbidden));
        ASSERT_TRUE(patchedTask.has_value());
        tasks.emplace_back(GetParam().expectedSuccess ? std::move(*patchedTask) : std::move(*task));
        ASSERT_NO_THROW((void) tasks.back().prescriptionId()) << tasks.back().serializeToJsonString();
    }

    void checkTaskStatus(const model::PrescriptionId& pid, model::Task::Status expected)
    {
        auto taskBundle = testBase.taskGetId(pid, kvnr.id());
        ASSERT_TRUE(taskBundle.has_value());
        std::optional<model::Task> task1;
        testBase.getTaskFromBundle(task1, *taskBundle);
        ASSERT_TRUE(task1.has_value());
        ASSERT_EQ(task1->status(), expected);
    }

    // checks the auditEvents from Setup() + what is passed here
    virtual void checkAuditEvents(const std::vector<std::optional<std::string>>& resourceIds,
                                  const std::vector<std::string>& actorIdentifiers,
                                  const std::unordered_set<std::size_t>& actorTelematicIdIndices,
                                  const std::vector<model::AuditEvent::SubType>& expectedActions)
    {
        std::vector<std::optional<std::string>> allResourceIds{"EUDISPCONS-" + kvnr.id(),
                                                               "eu-access-permission-" + kvnr.id() + "-FR",
                                                               tasks[0].prescriptionId().toString(),
                                                               tasks[0].prescriptionId().toString(),
                                                               tasks[1].prescriptionId().toString(),
                                                               tasks[1].prescriptionId().toString(),
                                                               tasks[2].prescriptionId().toString(),
                                                               tasks[2].prescriptionId().toString()};
        auto preEvents = allResourceIds.size();
        std::ranges::copy(resourceIds, std::back_inserter(allResourceIds));

        std::vector<std::string> allActorIdentifiers{kvnr.id(),   kvnr.id(), telematikId, kvnr.id(),
                                                     telematikId, kvnr.id(), telematikId, kvnr.id()};
        std::ranges::copy(actorIdentifiers, std::back_inserter(allActorIdentifiers));

        std::unordered_set<std::size_t> allActorTelematicIdIndices{2u, 4u, 6u};
        for (const std::size_t actorTelematicIdIndex : actorTelematicIdIndices)
        {
            allActorTelematicIdIndices.insert(actorTelematicIdIndex + preEvents);
        }

        std::vector<model::AuditEvent::SubType> allExpectedActions{
            model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::update,
            model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
            model::AuditEvent::SubType::update, model::AuditEvent::SubType::update};
        std::ranges::copy(expectedActions, std::back_inserter(allExpectedActions));

        if (GetParam().expectedSuccess)
        {
            testBase.checkAuditEvents(allResourceIds, kvnr.id(), "de", startTime, allActorIdentifiers,
                                      allActorTelematicIdIndices, allExpectedActions);
        }
    }

    std::vector<model::Task> tasks;
    model::Timestamp startTime = model::Timestamp::now();
    std::string telematikId = "0123456789";
    std::string ncpehId = "3-SMC-B-NCPEH-883110000120312";
};

TEST_P(ErpWorkflowEuTestGetPrescriptionsP, SetupOnly)
{
    checkAuditEvents({}, {}, {}, {});
}

TEST_P(ErpWorkflowEuTestGetPrescriptionsP, Demographics)
{
    ResourceTemplates::EuPostGetPrescriptionsOptions options{
        .version = ResourceTemplates::Versions::GEM_ERPEU_current(),
        .requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics,
        .kvnr = kvnr.id(),
        .accessCode = accessCode.toString().c_str(),
        .countryCode = "FR",
        .prescriptionIds = {}};
    std::shared_ptr<model::FhirResourceBase> response;
    ASSERT_NO_FATAL_FAILURE(response = postGetEuPrescriptions(options));
    ASSERT_NE(response, nullptr);
    if (GetParam().expectedSuccess)
    {
        auto bundle = dynamic_pointer_cast<model::Bundle>(response);
        const auto& responseRef = *response;
        ASSERT_NE(bundle, nullptr) << typeid(responseRef).name();
        ASSERT_EQ(bundle->getResourceCount(), 1);
    }
    else
    {
        ASSERT_EQ(response->getProfile(), model::ProfileType::OperationOutcome);
    }
    checkAuditEvents({std::nullopt}, {ncpehId}, {0}, {model::AuditEvent::SubType::read});
}

TEST_P(ErpWorkflowEuTestGetPrescriptionsP, PrescriptionsList)
{
    auto id1 = tasks[0].prescriptionId();
    auto id2 = tasks[1].prescriptionId();
    auto id3 = tasks[2].prescriptionId();
    ResourceTemplates::EuPostGetPrescriptionsOptions options{
        .version = ResourceTemplates::Versions::GEM_ERPEU_current(),
        .requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list,
        .kvnr = kvnr.id(),
        .accessCode = accessCode.toString().c_str(),
        .countryCode = "FR",
        .prescriptionIds = {}};

    std::shared_ptr<model::FhirResourceBase> response;
    ASSERT_NO_FATAL_FAILURE(response = postGetEuPrescriptions(options));
    ASSERT_NE(response, nullptr);
    if (GetParam().expectedSuccess)
    {
        auto bundle = dynamic_pointer_cast<model::Bundle>(response);
        ASSERT_NE(bundle, nullptr);
        ASSERT_EQ(bundle->getResourceCount(), 3);
    }
    else
    {
        ASSERT_EQ(response->getProfile(), model::ProfileType::OperationOutcome);
    }

    checkAuditEvents({std::nullopt}, {ncpehId}, {0}, {model::AuditEvent::SubType::read});

    // check task status not changed
    checkTaskStatus(id1, model::Task::Status::ready);
    checkTaskStatus(id2, model::Task::Status::ready);
    checkTaskStatus(id3, model::Task::Status::ready);
}

TEST_P(ErpWorkflowEuTestGetPrescriptionsP, PrescriptionsRetrieval)
{
    auto id1 = tasks[0].prescriptionId();
    auto id2 = tasks[1].prescriptionId();
    auto id3 = tasks[2].prescriptionId();
    ResourceTemplates::EuPostGetPrescriptionsOptions options{
        .version = ResourceTemplates::Versions::GEM_ERPEU_current(),
        .requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval,
        .kvnr = kvnr.id(),
        .accessCode = accessCode.toString().c_str(),
        .countryCode = "FR",
        .prescriptionIds = {id1.toString(), id2.toString()}};

    std::shared_ptr<model::FhirResourceBase> response;
    ASSERT_NO_FATAL_FAILURE(response = postGetEuPrescriptions(options));
    ASSERT_NE(response, nullptr);
    if (GetParam().expectedSuccess)
    {
        auto bundle = dynamic_pointer_cast<model::Bundle>(response);
        ASSERT_TRUE(bundle);
        ASSERT_EQ(bundle->getResourceCount(), 2);
    }
    else
    {
        ASSERT_EQ(response->getProfile(), model::ProfileType::OperationOutcome);
    }

    checkAuditEvents({std::nullopt}, {ncpehId}, {0}, {model::AuditEvent::SubType::update});

    // check task status changed to in-progress
    checkTaskStatus(id1, GetParam().expectedSuccess ? model::Task::Status::inprogress : model::Task::Status::ready);
    checkTaskStatus(id2, GetParam().expectedSuccess ? model::Task::Status::inprogress : model::Task::Status::ready);
    checkTaskStatus(id3, model::Task::Status::ready);
}


INSTANTIATE_TEST_SUITE_P(
    GetPrescriptions_featureToggle_expectedSuccess, ErpWorkflowEuTestGetPrescriptionsP,
    testing::Values(ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichigeArzneimittel, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisung, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::digitaleGesundheitsanwendungen, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisungPkv, true, false}));

class ErpWorkflowEuTestCloseTaskP : public ErpWorkflowEuTestGetPrescriptionsP
{
public:
    void SetUp() override
    {
        ErpWorkflowEuTestGetPrescriptionsP::SetUp();
        retrieveTasks();
    }

    void retrieveTasks()
    {
        ResourceTemplates::EuPostGetPrescriptionsOptions options{
            .version = ResourceTemplates::Versions::GEM_ERPEU_current(),
            .requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval,
            .kvnr = kvnr.id(),
            .accessCode = accessCode.toString().c_str(),
            .countryCode = "FR",
            .prescriptionIds = {tasks[0].prescriptionId().toString(), tasks[1].prescriptionId().toString(),
                                tasks[2].prescriptionId().toString()}};
        std::shared_ptr<model::FhirResourceBase> response;
        ASSERT_NO_FATAL_FAILURE(response = postGetEuPrescriptions(options));
        ASSERT_NE(response, nullptr);
        if (GetParam().expectedSuccess)
        {
            auto bundle = dynamic_pointer_cast<model::Bundle>(response);
            ASSERT_NE(bundle, nullptr);
            ASSERT_EQ(bundle->getResourceCount(), 3);
        }
        else
        {
            ASSERT_EQ(response->getProfile(), model::ProfileType::OperationOutcome);
        }
    }
};

TEST_P(ErpWorkflowEuTestCloseTaskP, closeTask)
{
    const auto getTask = [this] (const model::PrescriptionId& prescriptionId, const std::string& kvnr) -> std::pair<model::Bundle, model::Task> {
        std::optional<model::Bundle> taskBundle;
        EXPECT_NO_THROW(taskBundle = testBase.taskGetId(prescriptionId, kvnr));
        EXPECT_TRUE(taskBundle.has_value());
        EXPECT_GE(taskBundle->getResourceCount(), 1u); // At least one task resource.

        std::optional<model::Task> task;
        testBase.getTaskFromBundle(task, *taskBundle);
        EXPECT_TRUE(task.has_value());

        return std::make_pair<model::Bundle, model::Task>(std::move(taskBundle.value()), std::move(*task));
    };
    const auto lastModifiedDateBeforeClose = getTask(tasks[0].prescriptionId(), kvnr.id()).second.lastModifiedDate();

    const auto beforeClose = model::Timestamp::now() - model::Timestamp::duration_t(1h);
    ASSERT_NO_FATAL_FAILURE(postEuClose({.kvnr = kvnr.id(),
                                         .prescriptionId = tasks[0].prescriptionId().toString(),
                                         .whenHandedOver = model::Timestamp::now().toGermanDate(),
                                         .accessCode = accessCode.toString().c_str(),
                                         .countryCode = "FR"}));

    // retrieve (done in test setup), read (getTask) and then eu-close:
    checkAuditEvents({std::nullopt, tasks[0].prescriptionId().toString(), tasks[0].prescriptionId().toString()},
                     {ncpehId, kvnr.id(), ncpehId},
                     {0, 2},
                     {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::update});

    auto taskAndBundle = getTask(tasks[0].prescriptionId(), kvnr.id());
    ASSERT_EQ(taskAndBundle.first.getResourceCount(), 2u); // 0: task, 1: bundle (composition, patient, practitioner, organization, medication, coverage, medicationrequest)
    EXPECT_EQ(taskAndBundle.second.status(), GetParam().expectedSuccess ? model::Task::Status::completed : model::Task::Status::ready);
    ASSERT_EQ(taskAndBundle.second.lastMedicationDispense().has_value(), GetParam().expectedSuccess);
    if (GetParam().expectedSuccess)
    {
        EXPECT_GE(taskAndBundle.second.lastMedicationDispense(), beforeClose);
    }

    if (GetParam().expectedSuccess)
    {
        const auto lastModifiedAfterClose = taskAndBundle.second.lastModifiedDate();
        EXPECT_TRUE(lastModifiedDateBeforeClose.toChronoTimePoint() < lastModifiedAfterClose.toChronoTimePoint());
    }
}


TEST_P(ErpWorkflowEuTestCloseTaskP, closeTask_allDispensations)
{
    const auto beforeClose = model::Timestamp::now() - model::Timestamp::duration_t(1h);
    ASSERT_NO_FATAL_FAILURE(postEuClose({.kvnr = kvnr.id(),
                                         .prescriptionId = tasks[0].prescriptionId().toString(),
                                         .whenHandedOver = model::Timestamp::now().toGermanDate(),
                                         .accessCode = accessCode.toString().c_str(),
                                         .countryCode = "FR"}));

    // retrieve and then close:
    checkAuditEvents({std::nullopt, tasks[0].prescriptionId().toString()}, {ncpehId, ncpehId}, {0, 1},
                     {model::AuditEvent::SubType::update, model::AuditEvent::SubType::update});

    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_THROW(taskBundle = testBase.taskGetId(tasks[0].prescriptionId(), kvnr.id()));
    ASSERT_TRUE(taskBundle.has_value());
    ASSERT_EQ(taskBundle->getResourceCount(), 2u); // 0: task, 1: bundle (composition, patient, practitioner, organization, medication, coverage, medicationrequest)
    std::optional<model::Task> task;
    testBase.getTaskFromBundle(task, *taskBundle);
    ASSERT_TRUE(task.has_value());
    EXPECT_EQ(task->status(), GetParam().expectedSuccess ? model::Task::Status::completed : model::Task::Status::ready);
    ASSERT_EQ(task->lastMedicationDispense().has_value(), GetParam().expectedSuccess);
    if (GetParam().expectedSuccess)
    {
        EXPECT_GE(*task->lastMedicationDispense(), beforeClose);
    }

    // GET MedicationDispense/
    using namespace std::string_literals;
    constexpr auto copyToOriginalFormat = &model::NumberAsStringParserDocumentConverter::copyToOriginalFormat;
    const std::string getPath = "/MedicationDispense/";
    const auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    const ErpWorkflowTestBase::RequestArguments args{};
    const auto requestArguments = ErpWorkflowTestBase::RequestArguments{args}
                                      .withHttpMethod(HttpMethod::GET)
                                      .withVauPath(getPath)
                                      .withContentType(ContentMimeType::fhirJsonUtf8)
                                      .withJwt(jwt)
                                      .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                                      .withExpectedInnerStatus(HttpStatus::OK)
                                      .withExpectedBdeUseCase(bde::GetMedicationDispense_UC_3_9);

    ClientResponse serverResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = testBase.send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    const std::optional<model::Bundle>& medicationDispenseBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_TRUE(medicationDispenseBundle);
    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(copyToOriginalFormat(
                                                                 medicationDispenseBundle->jsonDocument()),
                                                             SchemaType::fhir));
    EXPECT_NO_FATAL_FAILURE(testutils::validate(*medicationDispenseBundle));
    std::vector<model::MedicationDispense> medicationDispenses;
    if (medicationDispenseBundle->getResourceCount() > 0)
    {
        medicationDispenses =
            medicationDispenseBundle->getResourcesByType<model::MedicationDispense>("MedicationDispense");
        for (const auto& md: medicationDispenses)
        {
            ASSERT_NO_THROW((void) model::MedicationDispense::fromXml(md.serializeToXmlString(), *StaticData::getXmlValidator()));

            const auto& practitionerRoles = medicationDispenseBundle->getResourcesByType<model::GemErpEuPrPractitionerRole>();
            const auto& practitioners = medicationDispenseBundle->getResourcesByType<model::GemErpEuPrPractitioner>();
            const auto& organizations = medicationDispenseBundle->getResourcesByType<model::GemErpEuPrOrganization>();

            EXPECT_EQ(practitionerRoles.size(), 1);
            EXPECT_EQ(practitioners.size(), 1);
            EXPECT_EQ(organizations.size(), 1);

            const auto& practitionerRole = practitionerRoles.front();
            const auto& practitioner = practitioners.front();
            const auto& organization = organizations.front();

            ASSERT_TRUE(md.performerReference().has_value());

            const auto& practitionerRoleId = Uuid{md.performerReference().value()}.toString();
            const auto& practitionerId = Uuid{practitionerRole.practitionerReference()}.toString();
            const auto& organizationId = Uuid{practitionerRole.organizationReference()}.toString();

            EXPECT_STREQ(std::string{practitionerRole.getId().value_or("")}.c_str(), practitionerRoleId.c_str());
            EXPECT_STREQ(std::string{practitioner.getId().value_or("")}.c_str(), practitionerId.c_str());
            EXPECT_STREQ(std::string{organization.getId().value_or("")}.c_str(), organizationId.c_str());

        }
    }
}

TEST_P(ErpWorkflowEuTestCloseTaskP, closeTasks_multipleDispensations)
{
    using namespace std::string_literals;
    constexpr auto copyToOriginalFormat = &model::NumberAsStringParserDocumentConverter::copyToOriginalFormat;

    const auto &kvnr = this->kvnr;
    auto &testBase = this->testBase;

    auto closeOperation = [&kvnr, &testBase]() {
        // Create task
        std::optional<model::Task> task;
        ASSERT_NO_FATAL_FAILURE(task = testBase.taskCreate());
        ASSERT_TRUE(task.has_value());
        std::string accessCode{task->accessCode()};
        auto prescriptionId{task->prescriptionId()};
        // Activate task
        const auto [qesBundle, _] = testBase.makeQESBundle(kvnr.id(), prescriptionId, model::Timestamp::now());
        ASSERT_NO_FATAL_FAILURE(testBase.taskActivateWithOutcomeValidation(prescriptionId, accessCode, qesBundle));
        // Accept task
        std::optional<model::Bundle> bundle;
        ASSERT_NO_FATAL_FAILURE(bundle = testBase.taskAccept(prescriptionId, accessCode));
        ASSERT_TRUE(bundle.has_value());
        auto tasksFromBundle = bundle->getResourcesByType<model::Task>("Task");
        ASSERT_EQ(tasksFromBundle.size(), 1);
        task.emplace(std::move(tasksFromBundle.front()));
        auto secret = task->secret();
        ASSERT_TRUE(secret.has_value());
        // Close task
        ASSERT_NO_FATAL_FAILURE(testBase.taskClose(prescriptionId, std::string{*secret}, kvnr.id()));
    };

    auto medicationDispenseOperation = [&kvnr, &testBase] (std::optional<model::Bundle>& medicationDispenseBundle) -> void {
        const auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        const ErpWorkflowTestBase::RequestArguments args{};
        const auto requestArguments =
            ErpWorkflowTestBase::RequestArguments{args}
                .withHttpMethod(HttpMethod::GET)
                .withVauPath("/MedicationDispense/")
                .withContentType(ContentMimeType::fhirJsonUtf8)
                .withJwt(jwt)
                .withHeader(Header::Authorization, testBase.getAuthorizationBearerValueForJwt(jwt))
                .withExpectedInnerStatus(HttpStatus::OK)
                .withExpectedBdeUseCase(bde::GetMedicationDispense_UC_3_9);

        ClientResponse serverResponse;
        EXPECT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = testBase.send(requestArguments));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        medicationDispenseBundle =
            model::Bundle::fromJsonNoValidation(serverResponse.getBody());
        ASSERT_TRUE(medicationDispenseBundle);
        EXPECT_NO_THROW(StaticData::getJsonValidator()->validate(
            copyToOriginalFormat(medicationDispenseBundle->jsonDocument()), SchemaType::fhir));
        EXPECT_NO_FATAL_FAILURE(testutils::validate(*medicationDispenseBundle));
    };

    // CLOSE
    closeOperation();

    // GET and CHECK MedicationDispensations
    std::optional<model::Bundle> bundle1;
    medicationDispenseOperation(bundle1);
    ASSERT_EQ(bundle1->getResourceCount(), 2);// MedicationDispense, Medication
    std::vector<model::MedicationDispense> medicationDispenses1 =
        bundle1->getResourcesByType<model::MedicationDispense>("MedicationDispense");
    ASSERT_EQ(medicationDispenses1.size(), 1);
    std::vector<model::GemErpPrMedication> medications1 =
        bundle1->getResourcesByType<model::GemErpPrMedication>("Medication");
    ASSERT_EQ(medications1.size(), 1);

    // EU-CLOSE
    ASSERT_NO_FATAL_FAILURE(postEuClose({.kvnr = kvnr.id(),
                                          .prescriptionId = tasks[0].prescriptionId().toString(),
                                          .whenHandedOver = model::Timestamp::now().toGermanDate(),
                                          .accessCode = accessCode.toString().c_str(),
                                          .countryCode = "FR"}));

    std::optional<model::Bundle> bundle2;
    medicationDispenseOperation(bundle2);
    const auto l = bundle2->getResourceCount();
    if (GetParam().expectedSuccess)
    {
        ASSERT_EQ(l, 7);
    }
    else
    {
        ASSERT_EQ(l, 2);
    }

    const auto& practitionerRoles =
        bundle2->getResourcesByType<model::GemErpEuPrPractitionerRole>();
    const auto& practitioners = bundle2->getResourcesByType<model::GemErpEuPrPractitioner>();
    const auto& organizations = bundle2->getResourcesByType<model::GemErpEuPrOrganization>();
    const auto& medicationDispenses = bundle2->getResourcesByType<model::MedicationDispense>();
    const auto& medications = bundle2->getResourcesByType<model::GemErpPrMedication>();

    if (GetParam().expectedSuccess)
    {
        ASSERT_EQ(medicationDispenses.size(), 2);
        ASSERT_EQ(medications.size(), 2);

        ASSERT_EQ(practitionerRoles.size(), 1);
        ASSERT_EQ(practitioners.size(), 1);
        ASSERT_EQ(organizations.size(), 1);

        const auto& practitionerRole = practitionerRoles.front();
        const auto& practitioner = practitioners.front();
        const auto& organization = organizations.front();

        const auto& practitionerId = Uuid{practitionerRole.practitionerReference()}.toString();
        const auto& organizationId = Uuid{practitionerRole.organizationReference()}.toString();

        for (const auto& md : medicationDispenses)
        {
            if (md.performerReference())
            {
                const auto& practitionerRoleId = Uuid{md.performerReference().value()}.toString();
                EXPECT_STREQ(std::string{practitionerRole.getId().value_or("")}.c_str(), practitionerRoleId.c_str());
                EXPECT_STREQ(std::string{practitioner.getId().value_or("")}.c_str(), practitionerId.c_str());
                EXPECT_STREQ(std::string{organization.getId().value_or("")}.c_str(), organizationId.c_str());
            }
        }
    }
    else
    {
        ASSERT_EQ(medicationDispenses.size(), 1);
        ASSERT_EQ(medications.size(), 1);

        ASSERT_EQ(practitionerRoles.size(), 0);
        ASSERT_EQ(practitioners.size(), 0);
        ASSERT_EQ(organizations.size(), 0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CloseTask_featureToggle_expectedSuccess, ErpWorkflowEuTestCloseTaskP,
    testing::Values(ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichigeArzneimittel, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, true, true},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisung, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::digitaleGesundheitsanwendungen, true, false},
                    ErpWorkflowEuTestParams{model::PrescriptionType::direkteZuweisungPkv, true, false}));
