/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Sha256.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"

namespace rv = model::ResourceVersion;


class CloseTaskTest : public EndpointHandlerTest
{
protected:
    enum class ExpectedResult
    {
        success,
        failure,
        noCatch,
    };

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
                ASSERT_ANY_THROW(handler.handleRequest(sessionContext));
                break;
            case ExpectedResult::noCatch:
                handler.handleRequest(sessionContext);
        }
    }

    fhirtools::Collection runRequest(const fhirtools::FhirStructureRepository& repo)
    {
        struct Element {
            Element(const fhirtools::FhirStructureRepository& repo, model::NumberAsStringParserDocument initDoc,
                    std::string elementId)
                : mElementId{std::move(elementId)}
                , mDocument{std::move(initDoc)}
                , mElement{std::make_shared<ErpElement>(&repo, std::weak_ptr<fhirtools::Element>{}, mElementId,
                                                        std::addressof(mDocument))}
            {
            }
            std::string mElementId;
            model::NumberAsStringParserDocument mDocument;
            std::shared_ptr<ErpElement> mElement;
        };
        CloseTaskHandler handler{{}};
        AccessLog accessLog;
        ServerRequest serverRequest = createRequest(
            ResourceTemplates::medicationDispenseXml({.prescriptionId = prescriptionId, .telematikId = telematikId}));
        ServerResponse serverResponse;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        [&] {
            ASSERT_NO_FATAL_FAILURE(handler.preHandleRequestHook(sessionContext););
            ASSERT_NO_FATAL_FAILURE(handler.handleRequest(sessionContext););
            ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        }();
        using BundleFactory = model::ResourceFactory<model::Bundle>;
        BundleFactory::Options opt;
        switch (Configuration::instance().prescriptionDigestRefType())
        {
            case Configuration::PrescriptionDigestRefType::relative:
                opt.validatorOptions = fhirtools::ValidatorOptions{
                    .levels =
                        {
                            .unreferencedBundledResource = fhirtools::Severity::warning,
                            .mandatoryResolvableReferenceFailure = fhirtools::Severity::warning,
                        },
                };
                break;
            case Configuration::PrescriptionDigestRefType::uuid:
                break;
        }
        auto doc =
            BundleFactory::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(), opt)
                .getValidated(SchemaType::fhir, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator());
        auto bundleElement = std::make_shared<Element>(repo, std::move(doc).jsonDocument(), "Bundle");
        return fhirtools::Collection{std::shared_ptr<fhirtools::Element>(bundleElement, bundleElement->mElement.get())};
    }

    fhirtools::Collection getPrescriptionDigest(const fhirtools::Collection& closeResponse)
    {
        [&] {
            ASSERT_EQ(closeResponse.size(), 1);
        }();

        const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
        auto prescriptionDigestExpr =
            fhirtools::FhirPathParser::parse(&repo, "Bundle.entry[0].resource.section.entry[0].resolve()");
        auto prescriptionDigest = prescriptionDigestExpr->eval(closeResponse);
        [&] {
            ASSERT_EQ(prescriptionDigest.size(), 1) << closeResponse;
        }();
        return prescriptionDigest;
    }

    JWT jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const std::string telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim).value();

    static const inline model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);

    void resetTask(SessionContext& sessionContext)
    {
        auto* database = sessionContext.database();
        auto taskAndKey = database->retrieveTaskForUpdate(prescriptionId);
        ASSERT_TRUE(taskAndKey.has_value());
        auto& task = taskAndKey->task;
        task.setStatus(model::Task::Status::inprogress);
        database->updateTaskStatusAndSecret(task);
        database->commitTransaction();
    }

};

TEST_F(CloseTaskTest, CloseTask)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    using BundleFactory = model::ResourceFactory<model::Bundle>;
    using ErxReceiptFactory = model::ResourceFactory<model::ErxReceipt>;
    A_22069.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto& testConfig = TestConfiguration::instance();

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
    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(dispenseOptions);
    const auto medicationDispenseBundleXml =
        ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {dispenseOptions, dispenseOptions}});

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
    {
        mockDatabase.reset();
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setBody(payload);
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        ASSERT_NO_THROW((void) BundleFactory::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator())
                .getValidated(SchemaType::Gem_erxReceiptBundle, *StaticData::getXmlValidator(),
                     *StaticData::getInCodeValidator(), model::ResourceVersion::supportedBundles()));
        TVLOG(0) << serverResponse.getBody();
        const auto receipt = ErxReceiptFactory::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator())
            .getValidated(SchemaType::Gem_erxReceiptBundle,*StaticData::getXmlValidator(), *StaticData::getInCodeValidator());
        const auto compositionResources = receipt.getResourcesByType<model::Composition>("Composition");
        ASSERT_EQ(compositionResources.size(), 1);
        const auto& composition = compositionResources.front();
        EXPECT_NO_THROW(static_cast<void>(composition.id()));
        EXPECT_TRUE(composition.telematikId().has_value());
        EXPECT_EQ(composition.telematikId().value(), jwtPharmacy.stringForClaim(JWT::idNumberClaim).value());
        EXPECT_TRUE(composition.periodStart().has_value());
        EXPECT_EQ(composition.periodStart().value().toXsDateTime(), "2021-02-02T16:18:43.036+00:00");
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

        std::optional<model::ErxReceipt> receiptFromSignature;
        ASSERT_NO_THROW(receiptFromSignature.emplace(
            model::ErxReceipt::fromXml(cms.payload(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                                       SchemaType::Gem_erxReceiptBundle)));
        EXPECT_FALSE(receiptFromSignature->getSignature().has_value());

        const std::string expectedFullUrl = "urn:uuid:" + std::string{composition.id()};
        const rapidjson::Pointer fullUrlPtr("/entry/0/fullUrl");
        std::optional<rapidjson::Document> originalFormatDocument;
        ASSERT_TRUE(originalFormatDocument = model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument()));
        ASSERT_TRUE(fullUrlPtr.Get(*originalFormatDocument));
        ASSERT_EQ(std::string{fullUrlPtr.Get(*originalFormatDocument)->GetString()}, expectedFullUrl);

        const std::string prescriptionDigestExpectedFullUrl{*prescriptionDigestIdentifier};
        const rapidjson::Pointer prescriptionDigestFullUrlPtr("/entry/2/fullUrl");
        ASSERT_TRUE(prescriptionDigestFullUrlPtr.Get(*originalFormatDocument));
        const auto prescriptionDigestActualFullUrl =
            std::string{prescriptionDigestFullUrlPtr.Get(*originalFormatDocument)->GetString()};

        static const rapidjson::Pointer prescriptionDigestIdPtr("/entry/2/resource/id");
        const auto* prescriptionDigestActualId = prescriptionDigestIdPtr.Get(*originalFormatDocument);
        ASSERT_NE(prescriptionDigestActualId, nullptr);
        ASSERT_TRUE(prescriptionDigestActualId->IsString());
        ASSERT_EQ("urn:uuid:"s += prescriptionDigestActualId->GetString(), prescriptionDigestIdentifier);

        EXPECT_EQ(prescriptionDigestActualFullUrl, prescriptionDigestExpectedFullUrl);
        ASSERT_TRUE(composition.prescriptionDigestIdentifier().has_value());
        const auto prescriptionDigestReference = std::string(*composition.prescriptionDigestIdentifier());
        EXPECT_NE(prescriptionDigestActualFullUrl.find(prescriptionDigestReference), std::string::npos);
        EXPECT_EQ(rapidjson::Pointer{"/entry/2/resource/meta/versionId"}.Get(*originalFormatDocument), nullptr);

        serverRequest.setPathParameters({"id"}, {"9a27d600-5a50-4e2b-98d3-5e05d2e85aa0"});
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);
    }

}

// Regression Test for Bugticket ERP-5656
TEST_F(CloseTaskTest, CloseTaskWrongMedicationDispenseErp5656)//NOLINT(readability-function-cognitive-complexity)
{
    A_22069.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
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
    serverRequest.setAccessToken(std::move(jwtPharmacy));
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
            ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {dispenseOptions, dispenseOptions}});
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "1970-01-01T00:00:00.000+00:00", date);
        ASSERT_NO_FATAL_FAILURE(test(std::move(medicationDispenseXml)));
    }
}

TEST_F(CloseTaskTest, ERP_13560_noMedicationDispenseGracePeriod)
{
    auto envGuard = testutils::getOverlappingFhirProfileEnvironment();
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1,
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
}

TEST_F(CloseTaskTest, noNewMedicationBefore20230701_single)
{
    auto envGuard = testutils::getOldFhirProfileEnvironment();
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1,
        .medication{.version = model::ResourceVersion::KbvItaErp::v1_1_0}};
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
}

TEST_F(CloseTaskTest, noNewMedicationBefore20230701_bundle)
{
    auto envGuard = testutils::getOldFhirProfileEnvironment();
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1,
        .medication{.version = model::ResourceVersion::KbvItaErp::v1_1_0}};
    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .medicationDispenses = {dispenseOptions, dispenseOptions}};
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
}

TEST_F(CloseTaskTest, MedicationDispenseWithoutVersion_2022)
{
    namespace rv = model::ResourceVersion;
    if (rv::supportedBundles() != std::set<rv::FhirProfileBundleVersion>{rv::FhirProfileBundleVersion::v_2022_01_01})
    {
        GTEST_SKIP_("Only relevant for 2022 profiles");
    }
    const auto profilePtr = rapidjson::Pointer{"/meta/profile/0"};
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1};
    const auto dispenseXml = medicationDispenseXml(dispenseOptions);
    auto dispenseModel =
        model::MedicationDispense::fromXml(dispenseXml, *StaticData::getXmlValidator(),
                                           *StaticData::getInCodeValidator(), SchemaType::Gem_erxMedicationDispense).jsonDocument();
    dispenseModel.setValue(profilePtr, model::resource::structure_definition::deprecated::medicationDispense);
    ASSERT_NO_FATAL_FAILURE(test(Fhir::instance().converter().jsonToXmlString(dispenseModel)));
}


TEST_F(CloseTaskTest, MedicationDispenseAcceptInvalid2022)
{
    // for 2022 profiles we have validated the medications only very loosely
    // make sure we still do that for the time before switching to the 2023 profiles
    EnvironmentVariableGuard environmentVariableGuardOld{ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE,
                                                         "disable"};
    namespace rv = model::ResourceVersion;
    if (! rv::supportedBundles().contains(rv::FhirProfileBundleVersion::v_2022_01_01))
    {
        GTEST_SKIP_("Only relevant for 2022 profiles");
    }
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .medication = {.version = rv::KbvItaErp::v1_0_2,
                       .templatePrefix = "test/EndpointHandlerTest/invalid_medication_wrong_code_template_"}};
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::success));
}


TEST_F(CloseTaskTest, MedicationDispenseBundleAcceptInvalid2022)
{
    // for 2022 profiles we have validated the medications only very loosely
    // make sure we still do that for the time before switching to the 2023 profiles
    EnvironmentVariableGuard environmentVariableGuardOld{ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE,
                                                         "disable"};
    namespace rv = model::ResourceVersion;
    if (! rv::supportedBundles().contains(rv::FhirProfileBundleVersion::v_2022_01_01))
    {
        GTEST_SKIP_("Only relevant for 2022 profiles");
    }
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .medication = {.version = rv::KbvItaErp::v1_0_2,
                       .templatePrefix = "test/EndpointHandlerTest/invalid_medication_wrong_code_template_"}};

    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .medicationDispenses = {dispenseOptions, dispenseOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::success));
}

TEST_F(CloseTaskTest, whenHandedOver)
{
    if (! rv::supportedBundles().contains(rv::FhirProfileBundleVersion::v_2023_07_01))
    {
        GTEST_SKIP_("Only relevant starting with overlapping period");
    }
    using namespace std::chrono_literals;
    const auto yesterday = model::Timestamp::now() - 24h;
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .whenHandedOver = yesterday,
    };

    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .medicationDispenses = {dispenseOptions, dispenseOptions}
    };
    // whenHandedOver during new profile period
    {
        EnvironmentVariableGuard newProfileValidFrom(ConfigurationKey::FHIR_PROFILE_VALID_FROM,
                                                     (yesterday - 24h).toXsDate(model::Timestamp::GermanTimezone));
        EnvironmentVariableGuard oldProfileValidUntil(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL,
                                                      (yesterday - 24h).toXsDate(model::Timestamp::GermanTimezone));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::success));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::success));
    }

    // whenHandedOver before overlapping period
    {
        EnvironmentVariableGuard newProfileValidFrom(ConfigurationKey::FHIR_PROFILE_VALID_FROM,
                                                     (yesterday + 24h).toXsDate(model::Timestamp::GermanTimezone));
        EnvironmentVariableGuard oldProfileValidUntil(ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL,
                                                      (yesterday + 24h).toXsDate(model::Timestamp::GermanTimezone));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
    }
    // whenHandedOver during new overlapping period
    {
        EnvironmentVariableGuard newProfileValidFrom(ConfigurationKey::FHIR_PROFILE_VALID_FROM,
                                                     (yesterday + 24h).toXsDate(model::Timestamp::GermanTimezone));
        EnvironmentVariableGuard oldProfileValidUntil(
            ConfigurationKey::FHIR_PROFILE_OLD_VALID_UNTIL,
            (model::Timestamp::now() + 24h).toXsDate(model::Timestamp::GermanTimezone));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
        ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
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

    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .medicationDispenses = {dispenseLaterOptions, dispenseNowOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::success));
}

TEST_F(CloseTaskTest, NoMetaVersionId)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE, "uuid");
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, std::nullopt);
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection prescriptionDigest;
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(runRequest(repo)));
    auto getMetaVersionId = fhirtools::FhirPathParser::parse(&repo, "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(prescriptionDigest);
    EXPECT_TRUE(metaVersionId.empty());
}

TEST_F(CloseTaskTest, SetMetaVersionId)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE, "uuid");
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, "isSet");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection prescriptionDigest;
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(runRequest(repo)));
    auto getMetaVersionId = fhirtools::FhirPathParser::parse(&repo, "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(prescriptionDigest);
    ASSERT_EQ(metaVersionId.size(), 1);
    EXPECT_EQ(metaVersionId.front()->asString(), "isSet");
}

TEST_F(CloseTaskTest, NoMetaVersionEmptyEnv)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE, "uuid");
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID, "");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection prescriptionDigest;
    ASSERT_NO_FATAL_FAILURE(prescriptionDigest = getPrescriptionDigest(runRequest(repo)));
    auto getMetaVersionId = fhirtools::FhirPathParser::parse(&repo, "meta.versionId");
    auto metaVersionId = getMetaVersionId->eval(prescriptionDigest);
    EXPECT_TRUE(metaVersionId.empty());
}

TEST_F(CloseTaskTest, prescriptionDigestRefTypeUUID)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE, "uuid");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = runRequest(repo));
    ASSERT_EQ(bundle.size(), 1);
    auto refInCompositionExpr =
        fhirtools::FhirPathParser::parse(&repo, "Bundle.entry[0].resource.section.entry[0].reference");
    auto refInComposition = refInCompositionExpr->eval(bundle);
    ASSERT_EQ(refInComposition.size(), 1) << bundle;
    EXPECT_TRUE(refInComposition.front()->asString().starts_with("urn:uuid:"));
    auto binaryResourceExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry.resource.ofType(Binary)");
    auto binary = binaryResourceExpr->eval(bundle);
    ASSERT_EQ(binary.size(), 1) << bundle;
    fhirtools::Collection binaryEntry{binary.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(&repo, "fullUrl");
    auto fullUrl = fullUrlExpr->eval(binaryEntry);
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), refInComposition.front()->asString());
    auto idExpr = fhirtools::FhirPathParser::parse(&repo, "id");
    auto id = idExpr->eval(binary);
    ASSERT_EQ(id.size(), 1) << binary;
    EXPECT_EQ("urn:uuid:" + id.front()->asString(), refInComposition.front()->asString()) << binary;
    EXPECT_TRUE(Uuid{id.front()->asString()}.isValidIheUuid()) << binary;
}

TEST_F(CloseTaskTest, prescriptionDigestRefTypeRelative)
{
    auto linkBase = Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL);
    std::string expectRef{"Binary/PrescriptionDigest-160.000.000.004.715.74"};
    auto expectFullUrl = linkBase + "/" + expectRef;
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_REF_TYPE, "relative");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = runRequest(repo));
    ASSERT_EQ(bundle.size(), 1);
    auto refInCompositionExpr =
        fhirtools::FhirPathParser::parse(&repo, "Bundle.entry[0].resource.section.entry[0].reference");
    auto refInComposition = refInCompositionExpr->eval(bundle);
    ASSERT_EQ(refInComposition.size(), 1) << bundle;
    EXPECT_EQ(refInComposition.front()->asString(), expectRef) << bundle;
    auto binaryResourceExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry.resource.ofType(Binary)");
    auto binary = binaryResourceExpr->eval(bundle);
    ASSERT_EQ(binary.size(), 1) << bundle;
    fhirtools::Collection binaryEntry{binary.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(&repo, "fullUrl");
    auto fullUrl = fullUrlExpr->eval(binaryEntry);
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), expectFullUrl);
    auto idExpr = fhirtools::FhirPathParser::parse(&repo, "id");
    auto id = idExpr->eval(binary);
    ASSERT_EQ(id.size(), 1) << binary;
    EXPECT_EQ("Binary/" + id.front()->asString(), refInComposition.front()->asString()) << binary;
}

TEST_F(CloseTaskTest, deviceRefUrl)
{
    auto linkBase = Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL);
    auto expectFullUrl = linkBase + "/Device/1";
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_DEVICE_REF_TYPE, "url");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = runRequest(repo));
    ASSERT_EQ(bundle.size(), 1);
    auto refInCompositionExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry[0].resource.author.reference");
    auto refInComposition = refInCompositionExpr->eval(bundle);
    ASSERT_EQ(refInComposition.size(), 1) << bundle;
    EXPECT_EQ(refInComposition.front()->asString(), expectFullUrl) << bundle;
    auto deviceResourceExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry.resource.ofType(Device)");
    auto device = deviceResourceExpr->eval(bundle);
    ASSERT_EQ(device.size(), 1) << bundle;
    fhirtools::Collection deviceEntry{device.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(&repo, "fullUrl");
    auto fullUrl = fullUrlExpr->eval(deviceEntry);
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), expectFullUrl);
    auto signatureWhoExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.signature.who.reference");
    auto signatureWho = signatureWhoExpr->eval(bundle);
    ASSERT_EQ(signatureWho.size(), 1);
    EXPECT_EQ(signatureWho.front()->asString(), expectFullUrl);
}

TEST_F(CloseTaskTest, deviceRefUuid)
{
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::SERVICE_TASK_CLOSE_DEVICE_REF_TYPE, "uuid");
    const auto& repo = Fhir::instance().structureRepository(model::ResourceVersion::currentBundle());
    fhirtools::Collection bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = runRequest(repo));
    ASSERT_EQ(bundle.size(), 1);
    auto refInCompositionExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry[0].resource.author.reference");
    auto refInComposition = refInCompositionExpr->eval(bundle);
    ASSERT_EQ(refInComposition.size(), 1) << bundle;
    EXPECT_TRUE(refInComposition.front()->asString().starts_with("urn:uuid:")) << bundle;
    auto deviceResourceExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.entry.resource.ofType(Device)");
    auto device = deviceResourceExpr->eval(bundle);
    ASSERT_EQ(device.size(), 1) << bundle;
    fhirtools::Collection deviceEntry{device.front()->parent()};
    auto fullUrlExpr = fhirtools::FhirPathParser::parse(&repo, "fullUrl");
    auto fullUrl = fullUrlExpr->eval(deviceEntry);
    ASSERT_EQ(fullUrl.size(), 1);
    EXPECT_EQ(fullUrl.front()->asString(), refInComposition.front()->asString());
    auto signatureWhoExpr = fhirtools::FhirPathParser::parse(&repo, "Bundle.signature.who.reference");
    auto signatureWho = signatureWhoExpr->eval(bundle);
    ASSERT_EQ(signatureWho.size(), 1);
    EXPECT_EQ(signatureWho.front()->asString(), fullUrl.front()->asString());
}

struct CloseTaskMixedProfileTestParam
{
    model::ResourceVersion::DeGematikErezeptWorkflowR4 dispenseVersion;
    model::ResourceVersion::KbvItaErp medicationVersion;
};

class CloseTaskMixedProfileTestProfileEnvironment
{
public:
    CloseTaskMixedProfileTestProfileEnvironment()
    {
        switch (::testing::WithParamInterface<CloseTaskMixedProfileTestParam>::GetParam().dispenseVersion)
        {
            case model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1:
                guards = ::testutils::getOldFhirProfileEnvironment();
                break;
            case model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_2_0:
                guards = ::testutils::getOverlappingFhirProfileEnvironment();
        }
    }

private:
    std::vector<EnvironmentVariableGuard> guards;
};

class CloseTaskMixedProfileTest : public ::testing::WithParamInterface<CloseTaskMixedProfileTestParam>,
                                  public CloseTaskMixedProfileTestProfileEnvironment,
                                  public CloseTaskTest
{
public:
    static std::string name(const testing::TestParamInfo<CloseTaskMixedProfileTestParam>& info)
    {

        std::ostringstream out;
        out << info.index << "_dispense_" << v_str(info.param.dispenseVersion) << "_medication_" << v_str(info.param.medicationVersion);
        return std::regex_replace(std::move(out).str(), std::regex{"[^A-Za-z0-9]"}, "_");
    }
};


TEST_P(CloseTaskMixedProfileTest, singleDispense)
{
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                 .telematikId = telematikId,
                                                                 .gematikVersion = GetParam().dispenseVersion,
                                                                 .medication{.version = GetParam().medicationVersion}};
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions)));
}

TEST_P(CloseTaskMixedProfileTest, multiDispense)
{
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                 .telematikId = telematikId,
                                                                 .gematikVersion = GetParam().dispenseVersion,
                                                                 .medication{.version = GetParam().medicationVersion}};
    auto bundleFhirBundleVersion = rv::fhirProfileBundleFromSchemaVersion(GetParam().dispenseVersion);
    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .bundleFhirBundleVersion = bundleFhirBundleVersion,
        .medicationDispenses = {dispenseOptions, dispenseOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions)));
}

TEST_P(CloseTaskMixedProfileTest, singleDispenseFail)
{
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = GetParam().dispenseVersion,
        .medication{.version = GetParam().medicationVersion,
                    .templatePrefix = "test/EndpointHandlerTest/invalid_medication_template_"}};
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseXml(dispenseOptions), ExpectedResult::failure));
}

TEST_P(CloseTaskMixedProfileTest, multiDispenseFail1)
{
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                 .telematikId = telematikId,
                                                                 .gematikVersion = GetParam().dispenseVersion,
                                                                 .medication{.version = GetParam().medicationVersion}};
    ResourceTemplates::MedicationDispenseOptions dispenseFailOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = GetParam().dispenseVersion,
        .medication{.version = GetParam().medicationVersion,
                    .templatePrefix = "test/EndpointHandlerTest/invalid_medication_template_"}};
    auto bundleFhirBundleVersion = rv::fhirProfileBundleFromSchemaVersion(GetParam().dispenseVersion);
    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .bundleFhirBundleVersion = bundleFhirBundleVersion,
        .medicationDispenses = {dispenseFailOptions, dispenseOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
}

TEST_P(CloseTaskMixedProfileTest, multiDispenseFail2)
{
    ResourceTemplates::MedicationDispenseOptions dispenseOptions{.prescriptionId = prescriptionId,
                                                                 .telematikId = telematikId,
                                                                 .gematikVersion = GetParam().dispenseVersion,
                                                                 .medication{.version = GetParam().medicationVersion}};
    ResourceTemplates::MedicationDispenseOptions dispenseFailOptions{
        .prescriptionId = prescriptionId,
        .telematikId = telematikId,
        .gematikVersion = GetParam().dispenseVersion,
        .medication{.version = GetParam().medicationVersion,
                    .templatePrefix = "test/EndpointHandlerTest/invalid_medication_template_"}};
    auto bundleFhirBundleVersion = rv::fhirProfileBundleFromSchemaVersion(GetParam().dispenseVersion);
    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .bundleFhirBundleVersion = bundleFhirBundleVersion,
        .medicationDispenses = {dispenseOptions, dispenseFailOptions}
    };
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
}


// clang-format off
INSTANTIATE_TEST_SUITE_P(good, CloseTaskMixedProfileTest,
    testing::ValuesIn<std::initializer_list<CloseTaskMixedProfileTestParam>>({
        {rv::DeGematikErezeptWorkflowR4::v1_1_1, rv::KbvItaErp::v1_0_2},
        {rv::DeGematikErezeptWorkflowR4::v1_2_0, rv::KbvItaErp::v1_0_2},
        {rv::DeGematikErezeptWorkflowR4::v1_2_0, rv::KbvItaErp::v1_1_0},
    }), &CloseTaskMixedProfileTest::name);
// clang-format on

class CloseTaskMixedMedicationVersionsTest
    : public CloseTaskTest
    , public ::testing::WithParamInterface<std::list<rv::KbvItaErp>>
{

};

TEST_P(CloseTaskMixedMedicationVersionsTest, run)
{
    A_23384.test("E-Rezept-Fachdienst: Prüfung Gültigkeit Profilversionen");
    ResourceTemplates::MedicationDispenseBundleOptions bundleOptions{
        .bundleFhirBundleVersion = rv::currentBundle(),
        .medicationDispenses = {}
    };
    for (const auto medicationVersion: GetParam())
    {
        bundleOptions.medicationDispenses.push_back({
            .prescriptionId = prescriptionId,
            .telematikId = telematikId,
            .medication {
                .version = medicationVersion
            }
        });
    }
    ASSERT_NO_FATAL_FAILURE(test(medicationDispenseBundleXml(bundleOptions), ExpectedResult::failure));
}

INSTANTIATE_TEST_SUITE_P(mixed, CloseTaskMixedMedicationVersionsTest,
    testing::ValuesIn<std::initializer_list<std::list<rv::KbvItaErp>>>({
        {rv::KbvItaErp::v1_0_2, rv::KbvItaErp::v1_1_0},
        {rv::KbvItaErp::v1_1_0, rv::KbvItaErp::v1_0_2},
        {rv::KbvItaErp::v1_1_0, rv::KbvItaErp::v1_0_2, rv::KbvItaErp::v1_1_0},
        {rv::KbvItaErp::v1_0_2, rv::KbvItaErp::v1_1_0, rv::KbvItaErp::v1_0_2},
    }));
