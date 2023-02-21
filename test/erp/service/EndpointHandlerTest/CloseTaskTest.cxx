/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Sha256.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ResourceTemplates.hxx"

namespace {
constexpr fhirtools::ValidatorOptions receiptValidationOptions
{
    .levels{
        .unreferencedBundledResource = fhirtools::Severity::warning,
        .unreferencedContainedResource = fhirtools::Severity::warning
    }
};
} // namespace


class CloseTaskTest : public EndpointHandlerTest
{
};

TEST_F(CloseTaskTest, CloseTask)//NOLINT(readability-function-cognitive-complexity)
{
    A_22069.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto& testConfig = TestConfiguration::instance();

    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const auto telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());

    CloseTaskHandler handler({});
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    const Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(
        {.prescriptionId = prescriptionId, .telematikId = telematikId.value()});
    const auto medicationDispenseBundleXml = ResourceTemplates::medicationDispenseBundleXml(
        {.prescriptionId = prescriptionId, .telematikId = telematikId.value()});

    ServerRequest serverRequest{requestHeader};
    serverRequest.setAccessToken(jwtPharmacy);
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});

    for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
    {
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setBody(payload);
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

        ASSERT_NO_THROW((void) model::Bundle::fromXml(
            serverResponse.getBody(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
            SchemaType::Gem_erxReceiptBundle, model::ResourceVersion::supportedBundles(), receiptValidationOptions));

        const model::ErxReceipt receipt = model::ErxReceipt::fromXml(
            serverResponse.getBody(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
            SchemaType::Gem_erxReceiptBundle, model::ResourceVersion::supportedBundles(), receiptValidationOptions);

        const auto compositionResources = receipt.getResourcesByType<model::Composition>("Composition");
        const auto deviceReferencePath = "/Device/" + std::to_string(model::Device::Id);
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
        EXPECT_EQ(UrlHelper::parseUrl(std::string(composition.author().value())).mPath, deviceReferencePath);

        const auto deviceResources = receipt.getResourcesByType<model::Device>("Device");
        ASSERT_EQ(deviceResources.size(), 1);
        const auto& device = deviceResources.front();
        EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion);
        EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion);
        const auto& bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId});
        const auto digest = Base64::encode(ByteHelper::fromHex(Sha256::fromBin(bundle)));
        const auto prescriptionDigest = receipt.prescriptionDigest();
        EXPECT_EQ(prescriptionDigest.data(), digest);

        const auto signature = receipt.getSignature();
        ASSERT_TRUE(signature.has_value());
        EXPECT_TRUE(signature->when().has_value());
        EXPECT_TRUE(signature->data().has_value());
        EXPECT_TRUE(signature->who().has_value());
        EXPECT_EQ(UrlHelper::parseUrl(std::string(signature->who().value())).mPath, deviceReferencePath);

        std::string signatureData;
        ASSERT_NO_THROW(signatureData = signature->data().value().data());
        const auto& cadesBesTrustedCertDir = testConfig.getOptionalStringValue(
            TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
        auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
        const CadesBesSignature cms(certs, signatureData);

        const model::ErxReceipt receiptFromSignature = model::ErxReceipt::fromXmlNoValidation(cms.payload());
        EXPECT_FALSE(receiptFromSignature.getSignature().has_value());


        const std::string expectedFullUrl = "urn:uuid:" + std::string{composition.id()};
        const rapidjson::Pointer fullUrlPtr("/entry/0/fullUrl");
        std::optional<rapidjson::Document> originalFormatDocument;
        ASSERT_TRUE(originalFormatDocument = model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument()));
        ASSERT_TRUE(fullUrlPtr.Get(*originalFormatDocument));
        ASSERT_EQ(std::string{fullUrlPtr.Get(*originalFormatDocument)->GetString()}, expectedFullUrl);

        const std::string prescriptionDigestExpectedFullUrl =
            Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL) +
            "/Binary/PrescriptionDigest-" + prescriptionId.toString();
        const rapidjson::Pointer prescriptionDigestFullUrlPtr("/entry/2/fullUrl");
        ASSERT_TRUE(prescriptionDigestFullUrlPtr.Get(*originalFormatDocument));
        const auto prescriptionDigestActualFullUrl =
            std::string{prescriptionDigestFullUrlPtr.Get(*originalFormatDocument)->GetString()};
        EXPECT_EQ(prescriptionDigestActualFullUrl, prescriptionDigestExpectedFullUrl);
        ASSERT_TRUE(composition.prescriptionDigestIdentifier().has_value());
        const auto prescriptionDigestReference = std::string(*composition.prescriptionDigestIdentifier());
        EXPECT_NE(prescriptionDigestActualFullUrl.find(prescriptionDigestReference), std::string::npos);

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
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const auto telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim);
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
                                                                    .telematikId = telematikId.value()};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml = ResourceTemplates::medicationDispenseBundleXml(settings);
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
    {// erroneous PrescriptionID
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = correctId,
                                                                    .telematikId = telematikId.value()};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml = ResourceTemplates::medicationDispenseBundleXml(settings);
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
                                                                    .kvnr = "X888888888",
                                                                    .telematikId = telematikId.value()};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml = ResourceTemplates::medicationDispenseBundleXml(settings);
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
    {// wrong Telematik-ID
        const ResourceTemplates::MedicationDispenseOptions settings{.prescriptionId = correctId,
                                                                    .kvnr = "X888888888",
                                                                    .telematikId = "falsch"};
        const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml(settings);
        const auto medicationDispenseBundleXml = ResourceTemplates::medicationDispenseBundleXml(settings);
        for (const auto& payload : {medicationDispenseXml, medicationDispenseBundleXml})
        {
            serverRequest.setBody(payload);
            ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
            EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
        }
    }
}

// Regression Test for Bugticket ERP-6513 (CloseTaskHandler does not accept MedicationDispense::whenPrepared and whenHandedOver with only date)
TEST_F(EndpointHandlerTest, CloseTaskPartialDateTimeErp6513)//NOLINT(readability-function-cognitive-complexity)
{
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    const auto telematikId = jwtPharmacy.stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());
    CloseTaskHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + prescriptionId.toString() + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    {
        auto medicationDispenseXml = ResourceTemplates::medicationDispenseBundleXml(
            {.prescriptionId = prescriptionId,
             .telematikId = telematikId.value(),
             .whenHandedOver = model::Timestamp::fromXsDate("1970-01-01"),
             .whenPrepared = model::Timestamp::fromXsDate("1970-01-02")});
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "1970-01-02T00:00:00.000+00:00", "2020-12");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "1970-01-01T00:00:00.000+00:00", "2020");
        serverRequest.setBody(std::move(medicationDispenseXml));
        EXPECT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_NO_THROW(handler.handleRequest(sessionContext));
    }
}
