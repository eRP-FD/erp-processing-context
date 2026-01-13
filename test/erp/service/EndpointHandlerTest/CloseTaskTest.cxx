/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ErxReceipt.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/service/task/DispenseTaskHandler.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/crypto/Sha256.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/MedicationDispenseFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string/replace.hpp>

class CloseTaskTest : public MedicationDispenseFixture
{
protected:

    ServerRequest createRequest(std::string body)
    {
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + prescriptionId.toString() + "/$close/",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setAccessToken(std::move(jwtPharmacy));
        serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
        serverRequest.setBody(std::move(body));
        return serverRequest;
    }

    void test(std::string body, ExpectedResult expectedResult = ExpectedResult::success)
    {

        CloseTaskHandler handler({});
        ServerRequest serverRequest = createRequest(std::move(body));
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_FATAL_FAILURE(resetTask(sessionContext));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        switch (expectedResult)
        {
            case ExpectedResult::success:
                ASSERT_NO_THROW(handler.handleRequest(sessionContext));
                break;
            case ExpectedResult::failure:
                // clang-format off
                ASSERT_NO_THROW(
                    try
                    {
                        handler.handleRequest(sessionContext);
                        ADD_FAILURE() << "Expected ErpException.";
                    }
                    catch (const ErpException& ex)
                    {
                        EXPECT_EQ(ex.status(), HttpStatus::BadRequest) << ex.what();
                    }
                );
                // clang-format on
                break;
            case ExpectedResult::noCatch:
                handler.handleRequest(sessionContext);
        }
    }

    fhirtools::EvaluationContext runRequest(const fhirtools::FhirStructureRepositoryView& repo)
    {
        struct Element {
            Element(const fhirtools::FhirStructureRepositoryBackend* repo, model::NumberAsStringParserDocument initDoc,
                    std::string elementId)
                : mElementId{std::move(elementId)}
                , mDocument{std::move(initDoc)}
                , mElement{std::make_shared<ErpElement>(repo, std::weak_ptr<fhirtools::Element>{}, mElementId,
                                                        std::addressof(mDocument))}
            {
            }
            std::string mElementId;
            model::NumberAsStringParserDocument mDocument;
            std::shared_ptr<ErpElement> mElement;
        };
        CloseTaskHandler handler{{}};
        AccessLog accessLog;
        ServerRequest serverRequest = createRequest(ResourceTemplates::dispenseOrCloseTaskBodyXml(
            {.version = ResourceTemplates::Versions::GEM_ERP_current(),
             .medicationDispenses = {{.prescriptionId = prescriptionId, .telematikId = telematikId}}}));
        ServerResponse serverResponse;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        [&] {
            ASSERT_NO_FATAL_FAILURE(handler.preHandleRequestHook(sessionContext););
            ASSERT_NO_FATAL_FAILURE(handler.handleRequest(sessionContext););
            ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        }();
        using BundleFactory = model::ResourceFactory<model::ErxReceipt>;
        BundleFactory::Options opt;
        auto doc = BundleFactory::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(), opt)
                       .getValidated(model::ProfileType::GEM_ERP_PR_Bundle);
        auto bundleElement = std::make_shared<Element>(std::addressof(Fhir::instance().backend()),
                                                       std::move(doc).jsonDocument(), "Bundle");
        return fhirtools::EvaluationContext{
            repo.shared_from_this(), std::shared_ptr<fhirtools::Element>(bundleElement, bundleElement->mElement.get())};
    }

    fhirtools::Collection getPrescriptionDigest(const fhirtools::EvaluationContext& closeResponse)
    {
        [&] {
            ASSERT_EQ(closeResponse.collection.size(), 1);
        }();

        const gsl::not_null repo = Fhir::instance().backend().defaultView();
        auto prescriptionDigestExpr = fhirtools::FhirPathParser::parse(
            std::addressof(Fhir::instance().backend()), "Bundle.entry[0].resource.section.entry[0].resolve()");
        auto prescriptionDigest = prescriptionDigestExpr->eval(closeResponse).collection;
        [&] {
            ASSERT_EQ(prescriptionDigest.size(), 1) << closeResponse.collection;
        }();
        return prescriptionDigest;
    }

    static const inline model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);

    void resetTask(SessionContext& sessionContext)
    {
        auto* database = sessionContext.database();
        auto taskAndKey = database->retrieveTaskForUpdate(prescriptionId);
        ASSERT_TRUE(taskAndKey.has_value());
        auto& task = taskAndKey->task;
        task.setStatus(model::Task::Status::inprogress);
        task.updateLastMedicationDispense(std::nullopt);
        database->updateTaskStatusAndSecret(task);
        database->commitTransaction();
    }

};


class CloseTaskInputTest : public CloseTaskTest , public testing::WithParamInterface<MedicationDispenseFixture::BodyType>
{
public:
    static constexpr char notSupported[] = "not supported";

public:
    const model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
};

TEST_P(CloseTaskInputTest, CloseTask)
{
    using namespace std::string_literals;
    A_22069_01.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto& testConfig = TestConfiguration::instance();

    CloseTaskHandler handler({});
    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    const auto& payload = medicationDispenseBody({EndpointType::close, GetParam()});
    if (payload == notSupported)
    {
        GTEST_SKIP_("input type not supported");
    }
    else
    {
        mockDatabase.reset();
        mServiceContext.databaseFactory(); // re-init mockdb
        auto task = mockDatabase->retrieveTaskForUpdate(prescriptionId);
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setBody(payload);
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext)) << serverRequest.getBody();
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);


        auto receipt = model::ErxReceipt::fromXmlNoValidation(serverResponse.getBody());
        ASSERT_NO_FATAL_FAILURE(testutils::validate(receipt));
        const auto compositionResources = receipt.getResourcesByType<model::Composition>("Composition");
        ASSERT_EQ(compositionResources.size(), 1);
        const auto& composition = compositionResources.front();
        EXPECT_NO_THROW(static_cast<void>(composition.id()));
        EXPECT_TRUE(composition.telematikId().has_value());
        EXPECT_EQ(composition.telematikId().value(), jwtPharmacy.stringForClaim(JWT::idNumberClaim).value());
        EXPECT_TRUE(composition.periodStart().has_value());
        EXPECT_EQ(composition.periodStart().value().toXsDateTime(), task->lastStatusChange.toXsDateTime());
        EXPECT_TRUE(composition.periodEnd().has_value());
        EXPECT_TRUE(composition.date().has_value());
        EXPECT_TRUE(composition.author().has_value());
        auto prescriptionDigestIdentifier = composition.prescriptionDigestIdentifier();
        ASSERT_TRUE(prescriptionDigestIdentifier.has_value());

        const auto deviceResources = receipt.getResourcesByType<model::Device>("Device");
        ASSERT_EQ(deviceResources.size(), 1);
        const auto& device = deviceResources.front();
        EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion());
        EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion());
        const auto& bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId});
        const auto digest = Base64::encode(ByteHelper::fromHex(Sha256::fromBin(bundle)));
        const auto prescriptionDigest = receipt.prescriptionDigest();
        EXPECT_EQ(prescriptionDigest.data(), digest);

        const auto signature = receipt.getSignature();
        ASSERT_TRUE(signature.has_value());
        EXPECT_TRUE(signature->when().has_value());
        EXPECT_TRUE(signature->data().has_value());
        EXPECT_TRUE(signature->whoReference().has_value());
        EXPECT_EQ(signature->whoReference().value(), composition.author().value());

        std::string signatureData;
        ASSERT_NO_THROW(signatureData = signature->data().value().data());
        const auto& cadesBesTrustedCertDir = testConfig.getOptionalStringValue(
            TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
        auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
        const CadesBesSignature cms(certs, signatureData);

        auto receiptFromSignature = model::ErxReceipt::fromXmlNoValidation(cms.payload());
        ASSERT_NO_FATAL_FAILURE(testutils::validate(receiptFromSignature));
        EXPECT_FALSE(receiptFromSignature.getSignature().has_value());

        const std::string expectedFullUrl = "urn:uuid:" + std::string{composition.id()};
        const rapidjson::Pointer fullUrlPtr("/entry/0/fullUrl");
        std::optional<rapidjson::Document> originalFormatDocument;
        ASSERT_TRUE(originalFormatDocument = model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument()));
        ASSERT_TRUE(fullUrlPtr.Get(*originalFormatDocument));
        ASSERT_EQ(std::string{fullUrlPtr.Get(*originalFormatDocument)->GetString()}, expectedFullUrl);

        serverRequest.setPathParameters({"id"}, {"9a27d600-5a50-4e2b-98d3-5e05d2e85aa0"});
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
    }
}

INSTANTIATE_TEST_SUITE_P(valid, CloseTaskInputTest,
                         testing::ValuesIn(magic_enum::enum_values<MedicationDispenseFixture::BodyType>()));

// Regression Test for Bugticket ERP-5656
TEST_F(CloseTaskTest, CloseTaskWrongMedicationDispenseErp5656)//NOLINT(readability-function-cognitive-complexity)
{
    A_22069_01.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto correctId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    CloseTaskHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + correctId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {correctId.toString()});
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    {// wrong PrescriptionID
        const auto wrongId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1111111);
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = wrongId,
                                                                    .telematikId = telematikId};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml =
            ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {settings, settings}});
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
    {// erroneous PrescriptionID
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = correctId,
                                                                    .telematikId = telematikId};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml =
            ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {settings, settings}});
        for (auto payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            payload = String::replaceAll(payload, correctId.toString(), "falsch");
            serverRequest.setBody(std::move(payload));
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
    {// wrong KVNR
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = correctId,
                                                                    .kvnr = "X888888880",
                                                                    .telematikId = telematikId};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml =
            ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {settings, settings}});
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
    {// wrong Telematik-ID
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = correctId,
                                                                    .kvnr = "X888888880",
                                                                    .telematikId = "falsch"};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml =
            ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {settings, settings}});
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
}

// Regression Test for Bugticket ERP-6513 (CloseTaskHandler does not accept MedicationDispense::whenPrepared and whenHandedOver with only date)
TEST_F(CloseTaskTest, CloseTaskPartialDateTimeErp6513)//NOLINT(readability-function-cognitive-complexity)
{
    for (const auto& date : {"2020-12-01", "2020-12", "2020"})
    {
        ResourceTemplates::MedicationDispenseOptions dispenseOptions{
            .prescriptionId = prescriptionId,
            .telematikId = telematikId,
            .whenPrepared = model::Timestamp::fromXsDate("1970-01-01", model::Timestamp::UTCTimezone)};
        auto medicationDispenseXml =
            ResourceTemplates::dispenseOrCloseTaskBodyXml({.version = ResourceTemplates::Versions::GEM_ERP_current(),
                                                           .medicationDispenses = {dispenseOptions, dispenseOptions}});
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "1970-01-01T00:00:00.000+00:00", date);
        ASSERT_NO_FATAL_FAILURE(test(std::move(medicationDispenseXml)));
    }
}

TEST_F(CloseTaskTest, WhenHandedOverNotTimestamp)
{
    A_22073_01.test("whenHandedOver not allowed as timestamp");
    std::list<ResourceTemplates::MedicationDispenseOptions> dispenseOptions{
        {.id{model::MedicationDispenseId{prescriptionId, 0}},
         .telematikId = telematikId,
         .whenHandedOver = "2025-04-17T15:50:00Z"},
    };
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
            .profileType = model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input,
            .medicationDispenses = dispenseOptions,
        });
    test(body, ExpectedResult::failure);
}

TEST_F(CloseTaskTest, WhenPreparedNotTimestamp)
{
    A_22073_01.test("whenPrepared not allowed as timestamp");
    std::list<ResourceTemplates::MedicationDispenseOptions> dispenseOptions{
            {.id{model::MedicationDispenseId{prescriptionId, 0}},
             .telematikId = telematikId,
             .whenPrepared = "2025-04-17T15:50:00Z"},
        };
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
            .profileType = model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input,
            .medicationDispenses = dispenseOptions,
        });
    test(body, ExpectedResult::failure);
}

TEST_F(CloseTaskTest, whenHandedOver)
{
    using namespace date::literals;
    auto today{date::floor<date::days>(std::chrono::system_clock::now())};
    const auto tomorrow = today + date::days{1};
    ResourceTemplates::MedicationOptions medicationOptions{
        .darreichungsform = "LYE",
    };
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .whenHandedOver = model::Timestamp{today},
        .medication = medicationOptions,
    };

    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .medicationDispenses = {dispenseOptions, dispenseOptions}};

    auto parameters = [dispenseOptions](const ResourceTemplates::Versions::GEM_ERP& gemVersion) mutable {
        dispenseOptions.gematikVersion = gemVersion;
        return ResourceTemplates::MedicationDispenseOperationParametersOptions{
            .version = gemVersion,
            .medicationDispenses = {dispenseOptions, dispenseOptions}};
    };
    auto parametersOptions1_4 = parameters(ResourceTemplates::Versions::GEM_ERP_1_4);
    auto parametersOptions1_5 = parameters(ResourceTemplates::Versions::GEM_ERP_1_5_2);

    // whenHandedOver during Workflow 1.2 period
    {
        testutils::ShiftFhirResourceViewsGuard shiftView{"2024-10-01", tomorrow};
        dispenseOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_2;
        bundleOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_2;
        bundleOptions.medicationDispenses.front().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_2;
        bundleOptions.medicationDispenses.back().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_2;
        // LYE is valid early for 1.2
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseXml(dispenseOptions), ExpectedResult::success));
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseBundleXml(bundleOptions), ExpectedResult::success));
        // failure is expected due to parameters only allowed with workflow 1.4
        EXPECT_NO_FATAL_FAILURE(test(medicationDispenseOperationParametersXml(parametersOptions1_4), ExpectedResult::failure));
    }
    // whenHandedOver during Workflow 1.3 period
    {
        testutils::ShiftFhirResourceViewsGuard shiftView{"2024-11-01", today};
        dispenseOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_3;
        bundleOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_3;
        bundleOptions.medicationDispenses.front().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_3;
        bundleOptions.medicationDispenses.back().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_3;
        // LYE is valid early for 1.3
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseBundleXml(bundleOptions), ExpectedResult::success));
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseXml(dispenseOptions), ExpectedResult::success));
        // failure is expected due to parameters only allowed with workflow 1.4
        EXPECT_NO_FATAL_FAILURE(test(medicationDispenseOperationParametersXml(parametersOptions1_4), ExpectedResult::failure));
    }
    // whenHandedOver during Workflow 1.4 period with Schlüsseltabellen Q2/2025 period
    {
        testutils::ShiftFhirResourceViewsGuard shiftView{"GEM_WF_1_4_NON_FDV", today};
        dispenseOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4;
        bundleOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4;
        bundleOptions.medicationDispenses.front().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4;
        bundleOptions.medicationDispenses.back().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4;
        // failure due to  MedicationDispense in versions 1.4 or later must be provided as Parameters
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
        // failure due to  MedicationDispense in versions 1.4 or later must be provided as Parameters
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
        // -LYE not valid yet- The view now allows LYE early.
        EXPECT_NO_FATAL_FAILURE(test(medicationDispenseOperationParametersXml(parametersOptions1_4), ExpectedResult::success));
    }
    // whenHandedOver during Workflow 1.5 period with Schlüsseltabellen Q2/2025 period
    {
        testutils::ShiftFhirResourceViewsGuard shiftView{"GEM_WF_1_5_0", today};
        dispenseOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_5_2;
        bundleOptions.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_5_2;
        bundleOptions.medicationDispenses.front().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_5_2;
        bundleOptions.medicationDispenses.back().gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_5_2;
        // failure due to  MedicationDispense in versions 1.4 or later must be provided as Parameters
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
        // failure due to  MedicationDispense in versions 1.4 or later must be provided as Parameters
        EXPECT_NO_FATAL_FAILURE(test(ResourceTemplates::medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
        // -LYE not valid yet- The view now allows LYE early.
        EXPECT_NO_FATAL_FAILURE(test(medicationDispenseOperationParametersXml(parametersOptions1_5), ExpectedResult::success));
    }
}

TEST_F(CloseTaskTest, whenHandedOverConsistentInBundle)
{
    // test that the whenHandedOver may have different dates (cf ANFERP-1847, ERP-16337, ANFERP-1995)
    using namespace std::chrono_literals;
    const auto now = model::Timestamp::now();
    const auto later = now + 1min;
    ResourceTemplates::MedicationDispenseOptions dispenseLaterOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .whenHandedOver = later,
    };

    ResourceTemplates::MedicationDispenseOptions dispenseNowOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .whenHandedOver = now,
    };

    ResourceTemplates::MedicationDispenseOperationParametersOptions options{
        .version = ResourceTemplates::Versions::GEM_ERP_current(),
        .medicationDispenses = {dispenseLaterOptions, dispenseNowOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(dispenseOrCloseTaskBodyXml(options), ExpectedResult::success));
}

TEST_F(CloseTaskTest, NoEnvMetaVersionId)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, std::nullopt);
    const gsl::not_null repo = Fhir::instance().backend().defaultView();
    fhirtools::Collection prescriptionDigest;
    auto bundleContext = runRequest(*repo);
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(bundleContext));
    auto getMetaVersionId =
        fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(bundleContext(prescriptionDigest)).collection;
    ASSERT_EQ(metaVersionId.size(), 1);
    EXPECT_EQ(metaVersionId.front()->asString(), "1");
}

TEST_F(CloseTaskTest, SetMetaVersionId)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, "isSet");
    const gsl::not_null repo = Fhir::instance().backend().defaultView();
    fhirtools::Collection prescriptionDigest;
    auto bundleContext = runRequest(*repo);
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(bundleContext));
    auto getMetaVersionId =
        fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(bundleContext(prescriptionDigest)).collection;
    ASSERT_EQ(metaVersionId.size(), 1);
    EXPECT_EQ(metaVersionId.front()->asString(), "isSet");
}

TEST_F(CloseTaskTest, NoMetaVersionEmptyEnv)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, "");
    const gsl::not_null repo = Fhir::instance().backend().defaultView();
    fhirtools::Collection prescriptionDigest;
    auto bundleContext = runRequest(*repo);
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(bundleContext));
    auto getMetaVersionId =
        fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(bundleContext(prescriptionDigest));
    EXPECT_TRUE(metaVersionId.collection.empty());
}

TEST_F(CloseTaskTest, prescriptionDigestRefTypeUUID)
{
    std::vector<EnvironmentVariableGuard> guards;
    const gsl::not_null repo = Fhir::instance().backend().defaultView();
    std::optional<fhirtools::EvaluationContext> bundleContext;
    ASSERT_NO_FATAL_FAILURE(bundleContext.emplace(runRequest(*repo)));
    ASSERT_EQ(bundleContext->collection.size(), 1);
    auto refInCompositionExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()),
                                                                 "Bundle.entry[0].resource.section.entry[0].reference");
    auto refInComposition = refInCompositionExpr->eval(*bundleContext);
    ASSERT_EQ(refInComposition.collection.size(), 1) << bundleContext->collection;
    EXPECT_TRUE(refInComposition.collection.front()->asString().starts_with("urn:uuid:"));
    auto binaryResourceExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()),
                                                               "Bundle.entry.resource.ofType(Binary)");
    auto binary = binaryResourceExpr->eval(*bundleContext);
    ASSERT_EQ(binary.collection.size(), 1) << bundleContext->collection;
    fhirtools::EvaluationContext binaryEntry{repo, binary.collection.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "fullUrl");
    auto fullUrl = fullUrlExpr->eval(binaryEntry).collection;
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), refInComposition.collection.front()->asString());
    auto idExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "id");
    auto id = idExpr->eval(binary).collection;
    ASSERT_EQ(id.size(), 1) << binary.collection;
    EXPECT_EQ("urn:uuid:" + id.front()->asString(), refInComposition.collection.front()->asString()) << binary.collection;
    EXPECT_TRUE(Uuid{id.front()->asString()}.isValidIheUuid()) << binary.collection;
}

TEST_F(CloseTaskTest, deviceRefUuid)
{
    std::vector<EnvironmentVariableGuard> guards;
    const gsl::not_null repo = Fhir::instance().backend().defaultView();
    std::optional<fhirtools::EvaluationContext> bundleContext;
    ASSERT_NO_FATAL_FAILURE(bundleContext.emplace(runRequest(*repo)));
    ASSERT_EQ(bundleContext->collection.size(), 1);
    auto refInCompositionExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()),
                                                                 "Bundle.entry[0].resource.author.reference");
    auto refInComposition = refInCompositionExpr->eval(*bundleContext).collection;
    ASSERT_EQ(refInComposition.size(), 1) << bundleContext->collection;
    EXPECT_TRUE(refInComposition.front()->asString().starts_with("urn:uuid:")) << bundleContext->collection;
    auto deviceResourceExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()),
                                                               "Bundle.entry.resource.ofType(Device)");
    auto device = deviceResourceExpr->eval(*bundleContext);
    ASSERT_EQ(device.collection.size(), 1) << bundleContext->collection;
    fhirtools::EvaluationContext deviceEntry{repo, device.collection.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "fullUrl");
    auto fullUrl = fullUrlExpr->eval(deviceEntry).collection;
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), refInComposition.front()->asString());
    auto signatureWhoExpr =
        fhirtools::FhirPathParser::parse(std::addressof(Fhir::instance().backend()), "Bundle.signature.who.reference");
    auto signatureWho = signatureWhoExpr->eval(*bundleContext).collection;
    ASSERT_EQ(signatureWho.size(), 1);
    EXPECT_EQ(signatureWho.front()->asString(), fullUrl.front()->asString());
}

TEST_F(CloseTaskTest, EmptyBody_NoMedicationDispense)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    A_24287.test("Aufruf ohne MedicationDispense im Request Body");
    CloseTaskHandler handler({});
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    mockDatabase.reset();
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::Forbidden);
}

TEST_F(CloseTaskTest, EmptyBody_WithMedicationDispense)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace ResourceTemplates;
    if (Versions::GEM_ERP_current() < Versions::GEM_ERP_1_3)
    {
        GTEST_SKIP();
    }
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4718);

    using namespace std::string_literals;
    A_24287.test("Aufruf ohne MedicationDispense im Request Body");
    CloseTaskHandler handler({});
    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    mockDatabase.reset();
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
}

TEST_F(CloseTaskTest, ErpExceptionWithOldProfile)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;

    CloseTaskHandler handler({});
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto medicationDispenseBundleXml =
        ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {dispenseOptions, dispenseOptions}});

    /* ERP-22297: remove meta/profile */
    const std::string medicationDispenseBundleProfile{profile(model::ProfileType::MedicationDispenseBundle).value()};
    std::string medicationDispenseBundleXmlNoProfile = regex_replace(medicationDispenseBundleXml,
        std::regex{"<meta><profile value=\"" +
        medicationDispenseBundleProfile + "\\|" + to_string(ResourceTemplates::Versions::GEM_ERP_current()) +
        "\"/></meta>"}, "");
    EXPECT_TRUE(medicationDispenseBundleXmlNoProfile.find("<meta><profile value="
        "\"https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_CloseOperationInputBundle") == std::string::npos);

    /* ERP-22706: MedicationDispense with old meta/profile */
    const std::string medicationDispenseProfile{profile(model::ProfileType::GEM_ERP_PR_MedicationDispense).value()};
    std::string medicationDispenseBundleXmlOldProfile =
        String::replaceAll(medicationDispenseBundleXmlNoProfile,
                           medicationDispenseProfile + "|" + to_string(ResourceTemplates::Versions::GEM_ERP_current()),
                           medicationDispenseProfile + "|1.0");

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    for (const auto& payload : {medicationDispenseBundleXmlOldProfile})
    {
        mockDatabase.reset();
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setBody(payload);
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_THROW(handler.handleRequest(sessionContext), ErpException);
    }
}

TEST(CloseTaskConfigTest, defaultValues)
{
    std::vector<EnvironmentVariableGuard> unsetEnv;
    unsetEnv.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, std::nullopt);
    const auto& config = Configuration::instance();
    const auto& metaVersionId = config.getOptionalStringValue(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID);
    ASSERT_TRUE(metaVersionId.has_value());
    EXPECT_EQ(*metaVersionId, "1");
}

TEST_F(CloseTaskTest, CloseTask_FutureLastStatusUpdate)
{
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4799);

    // Create a task with last_status_update later than now() in order to simulate a clock skew.
    {
        const auto lastStatusChange = model::Timestamp::now() + std::chrono::minutes(5);

        auto taskY = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = prescriptionId}));

        model::Task taskX(taskY.prescriptionId(), taskY.type(), taskY.lastModifiedDate(), taskY.authoredOn(),
                          taskY.status(), lastStatusChange);
        taskX.setSecret(*taskY.secret());
        taskX.setKvnr(*taskY.kvnr());

        const auto& kbvBundleX = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId}));
        const auto healthCarePrescriptionUuidX = taskY.healthCarePrescriptionUuid().value();
        const auto healthCarePrescriptionBundleX = model::Binary(healthCarePrescriptionUuidX, kbvBundleX).serializeToJsonString();

        mockDatabase->insertTask(taskX, std::nullopt, healthCarePrescriptionBundleX);
    }

    CloseTaskHandler handler({});

    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};

    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = ResourceTemplates::dispenseOrCloseTaskBodyXml(
        {.version = ResourceTemplates::Versions::GEM_ERP_current(), .medicationDispenses = {dispenseOptions}});

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        handler.handleRequest(sessionContext), HttpStatus::InternalServerError,
        "in-progress date later than completed time.");

    mockDatabase.reset();
}

TEST_F(CloseTaskTest, CloseTask_IgnoreFutureLastModified)
{
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4799);

    // Create a task with lastModified later than now() in order to simulate a clock skew.
    {
        const auto lastModified = model::Timestamp::now() + std::chrono::minutes(5);

        const auto taskX = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = prescriptionId,
             .timestamp = lastModified}));

        const auto& kbvBundleX = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId}));
        const auto healthCarePrescriptionUuidX = taskX.healthCarePrescriptionUuid().value();
        const auto healthCarePrescriptionBundleX = model::Binary(healthCarePrescriptionUuidX, kbvBundleX).serializeToJsonString();

        mockDatabase->insertTask(taskX, std::nullopt, healthCarePrescriptionBundleX);
    }
    CloseTaskHandler handler({});

    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};

    const ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                       .telematikId = telematikId};
    const auto body = ResourceTemplates::dispenseOrCloseTaskBodyXml(
        {.version = ResourceTemplates::Versions::GEM_ERP_current(), .medicationDispenses = {dispenseOptions}});

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    EXPECT_NO_THROW(handler.handleRequest(sessionContext));

    mockDatabase.reset();
}

TEST_F(CloseTaskTest, DigaRedeemCodeHeader1)
{
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002);
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
        .medicationDispenses = {},
        .medicationDispensesDiGA =
            {
                {
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .prescriptionId = prescriptionId.toString(),
                    .withRedeemCode = true,
                },
            },
    });
    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};
    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtKostentraeger());
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});

    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    CloseTaskHandler handler({});
    handler.handleRequest(sessionContext);

    ASSERT_TRUE(sessionContext.getOuterResponseHeaderFields().contains(Header::DigaRedeemCode));
    EXPECT_EQ(sessionContext.getOuterResponseHeaderFields().at(Header::DigaRedeemCode), "1");
}

TEST_F(CloseTaskTest, UnslicedExtensionAllow)
{
    A_22927_02.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard envGuard{"ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM",
                                      (model::Timestamp::now() + std::chrono::days{1}).toGermanDate()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002);
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
        .medicationDispenses = {},
        .medicationDispensesDiGA =
            {
                {
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .prescriptionId = prescriptionId.toString(),
                    .withRedeemCode = true,
                },
            },
    });
    body = String::replaceAll(body, "<meta>",
                              R"(<meta><extension url="subStatus"><valueBoolean value="true"/></extension>)");
    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};
    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtKostentraeger());
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});

    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    CloseTaskHandler handler({});
    handler.handleRequest(sessionContext);
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
}

TEST_F(CloseTaskTest, UnslicedExtensionReject)
{
    A_22927_02.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard envGuard{"ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM",
                                      (model::Timestamp::now()).toGermanDate()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002);
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
        .medicationDispenses = {},
        .medicationDispensesDiGA =
            {
                {
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .prescriptionId = prescriptionId.toString(),
                    .withRedeemCode = true,
                },
            },
    });
    boost::replace_first(body, "<meta>", R"(<meta><extension url="subStatus"><valueBoolean value="true"/></extension>)");
    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};
    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtKostentraeger());
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});

    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    CloseTaskHandler handler({});
    if (ResourceTemplates::Versions::KBV_ERP_current() >= ResourceTemplates::Versions::KBV_ERP_1_3_3)
    {
        std::string diagnostics =
            "Parameters.meta.extension[0]: error: element doesn't belong to any slice. (from profile: "
            "http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); Parameters.meta.extension[0]: error: element "
            "doesn't belong to any slice. (from profile: http://hl7.org/fhir/StructureDefinition/Meta|4.0.1); "
            "Parameters.meta.extension[0]: error: element doesn't belong to any slice. (from profile: "
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CloseOperation_Input|1.5.2); ";
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(
            handler.handleRequest(sessionContext), "Unintendierte Verwendung von Extensions an unspezifizierter Stelle",
            diagnostics);
    }
    else
    {
        std::string diagnostics =
            "Parameters.meta.extension[0]: error: element doesn't belong to any slice. (from profile: "
            "http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); Parameters.meta.extension[0]: error: element "
            "doesn't belong to any slice. (from profile: http://hl7.org/fhir/StructureDefinition/Meta|4.0.1); "
            "Parameters.meta.extension[0]: error: element doesn't belong to any slice. (from profile: "
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CloseOperation_Input|1.4); ";
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(
            handler.handleRequest(sessionContext), "Unintendierte Verwendung von Extensions an unspezifizierter Stelle",
            diagnostics);
    }
}


TEST_F(CloseTaskTest, DigaRedeemCodeHeader0)
{
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002);
    auto body = ResourceTemplates::medicationDispenseOperationParametersXml({
        .medicationDispenses = {},
        .medicationDispensesDiGA =
            {
                {
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .prescriptionId = prescriptionId.toString(),
                    .withRedeemCode = false,
                },
            },
    });
    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId.toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};
    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(mJwtBuilder->makeJwtKostentraeger());
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});

    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    CloseTaskHandler handler({});
    handler.handleRequest(sessionContext);

    ASSERT_TRUE(sessionContext.getOuterResponseHeaderFields().contains(Header::DigaRedeemCode));
    EXPECT_EQ(sessionContext.getOuterResponseHeaderFields().at(Header::DigaRedeemCode), "0");
}

class CloseTaskWrongInputTest : public CloseTaskTest, public testing::WithParamInterface<model::PrescriptionType>
{
};
TEST_P(CloseTaskWrongInputTest, test)
{
    A_26002.test("Task schließen - Flowtype 160/169/200/209 - Profilprüfung MedicationDispense");
    A_26003.test("Task schließen - Flowtype 162 - Profilprüfung MedicationDispense");
    std::optional<model::PrescriptionId> prescriptionId1{prescriptionId};
    std::string message =
        "Unzulässige Abgabeinformationen: Für diesen Workflow sind nur Abgabeinformationen für Arzneimittel zulässig.";
    JWT jwt = jwtPharmacy;
    ResourceTemplates::MedicationDispenseOperationParametersOptions medicationDispenseOptions;
    switch (GetParam())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv: {
            medicationDispenseOptions.medicationDispenses.clear();
            auto& md = medicationDispenseOptions.medicationDispensesDiGA.emplace_front();
            md.prescriptionId = prescriptionId1->toString();
            md.version = medicationDispenseOptions.version;
            md.whenHandedOver = model::Timestamp::now().toGermanDate();
            break;
        }
        case model::PrescriptionType::digitaleGesundheitsanwendungen: {
            prescriptionId1.emplace(
                model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002));
            message = "Unzulässige Abgabeinformationen: Für diesen Workflow sind nur Abgabeinformationen für digitale "
                      "Gesundheitsanwendungen zulässig.";
            jwt = mJwtBuilder->makeJwtKostentraeger();
            auto& md = medicationDispenseOptions.medicationDispenses.emplace_front(ResourceTemplates::MedicationDispenseOptions{
                .prescriptionId = *prescriptionId1,
                .gematikVersion = medicationDispenseOptions.version,
            });
            md.medication = ResourceTemplates::MedicationOptions{.version = medicationDispenseOptions.version};
            break;
        }
    }
    std::string body = ResourceTemplates::medicationDispenseOperationParametersXml(medicationDispenseOptions);
    CloseTaskHandler handler({});
    const Header requestHeader{HttpMethod::POST,
                               "/Task/" + prescriptionId1->toString() + "/$close/",
                               0,
                               {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                               HttpStatus::Unknown};

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwt);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    serverRequest.setPathParameters({"id"}, {prescriptionId1->toString()});
    serverRequest.setBody(body);
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::BadRequest, message.c_str());

    mockDatabase.reset();
}
INSTANTIATE_TEST_SUITE_P(x, CloseTaskWrongInputTest,
                         testing::ValuesIn(magic_enum::enum_values<model::PrescriptionType>()));


class CloseTaskProfileValidityTest : public CloseTaskTest,
public ::testing::WithParamInterface< MedicationDispenseFixture::ProfileValidityTestParams>
{
};

TEST_P(CloseTaskProfileValidityTest, run)
{
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBody({EndpointType::close, GetParam().bodyType,
        GetParam().version, {testutils::shiftDate(GetParam().date)}}),
                                 GetParam().result));
}

INSTANTIATE_TEST_SUITE_P(parameters, CloseTaskProfileValidityTest,
                         ::testing::ValuesIn(MedicationDispenseFixture::profileValidityTestParameters()));


class CloseTaskMaxWhenHandedOverTest : public CloseTaskTest,
public ::testing::WithParamInterface< MedicationDispenseFixture::MaxWhenHandedOverTestParams>
{
};

TEST_P(CloseTaskMaxWhenHandedOverTest, run)
{
    std::list<std::string> overrideWhenHandedOver;
    std::ranges::transform(GetParam().whenHandedOver, back_inserter(overrideWhenHandedOver), &testutils::shiftDate);
    ASSERT_NO_FATAL_FAILURE(test(
        medicationDispenseBody({EndpointType::close, GetParam().bodyType, GetParam().version, overrideWhenHandedOver}),
        GetParam().result));
}

INSTANTIATE_TEST_SUITE_P(parameters, CloseTaskMaxWhenHandedOverTest,
                         ::testing::ValuesIn(MedicationDispenseFixture::maxWhenHandedOverTestParameters()));
