/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "erp/ErpRequirements.hxx"


class CloseTaskTestP : public EndpointHandlerTest, public testing::WithParamInterface<std::string>
{
};

TEST_P(CloseTaskTestP, CloseTask)//NOLINT(readability-function-cognitive-complexity)
{
    A_22069.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto& testConfig = TestConfiguration::instance();
    auto medicationDispenseXml = FileHelper::readFileAsString(GetParam());

    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();

    CloseTaskHandler handler({});
    const auto id =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", id);
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + id + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {id});
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    serverRequest.setBody(std::move(medicationDispenseXml));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    ASSERT_NO_THROW((void) model::Bundle::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
                                                  *StaticData::getInCodeValidator(), SchemaType::Gem_erxReceiptBundle));

    model::ErxReceipt receipt =
        model::ErxReceipt::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(),
                                   *StaticData::getInCodeValidator(), SchemaType::Gem_erxReceiptBundle);

    jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();// need to be newly created as moved to access token

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

    const auto prescriptionDigest = receipt.prescriptionDigest();
    EXPECT_EQ(prescriptionDigest.data(), "uqQu3nvsTNw7Gz97jkAuCzSebWyvZ4FZ5zE7TQTdxg0=");

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
    CadesBesSignature cms(certs, signatureData);

    model::ErxReceipt receiptFromSignature = model::ErxReceipt::fromXmlNoValidation(cms.payload());
    EXPECT_FALSE(receiptFromSignature.getSignature().has_value());

    serverRequest.setPathParameters({"id"}, {"9a27d600-5a50-4e2b-98d3-5e05d2e85aa0"});
    EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::NotFound);


    std::string expectedFullUrl = "urn:uuid:" + std::string{composition.id()};
    rapidjson::Pointer fullUrlPtr("/entry/0/fullUrl");
    ASSERT_TRUE(
        fullUrlPtr.Get(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument())));
    ASSERT_EQ(
        std::string{
            fullUrlPtr.Get(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(receipt.jsonDocument()))
                ->GetString()},
        expectedFullUrl);
}

// Regression Test for Bugticket ERP-5656
TEST_P(CloseTaskTestP, CloseTaskWrongMedicationDispenseErp5656)//NOLINT(readability-function-cognitive-complexity)
{
    A_22069.test("Test is parameterized with MedicationDispense and MedicationDispenseBundle Resource");
    const auto correctId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    const auto wrongId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1111111)
            .toString();
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    CloseTaskHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + correctId + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {correctId});
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    {// wrong PrescriptionID
        auto medicationDispenseXml = FileHelper::readFileAsString(GetParam());
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", wrongId);
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    {// erroneous PrescriptionID
        auto medicationDispenseXml = FileHelper::readFileAsString(GetParam());
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", "falsch");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    {// wrong KVNR
        auto medicationDispenseXml = FileHelper::readFileAsString(GetParam());
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "X234567890", "X888888888");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
    {// wrong Telematik-ID
        auto medicationDispenseXml = FileHelper::readFileAsString(GetParam());
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml =
            String::replaceAll(medicationDispenseXml, "3-SMC-B-Testkarte-883110000120312", "falsch");
        serverRequest.setBody(std::move(medicationDispenseXml));
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_ERP_EXCEPTION(handler.handleRequest(sessionContext), HttpStatus::BadRequest);
    }
}

// Regression Test for Bugticket ERP-6513 (CloseTaskHandler does not accept MedicationDispense::whenPrepared and whenHandedOver with only date)
TEST_F(EndpointHandlerTest, CloseTaskPartialDateTimeErp6513)//NOLINT(readability-function-cognitive-complexity)
{
    const auto correctId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715).toString();
    auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    CloseTaskHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/Task/" + correctId + "/$close/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"}, {correctId});
    serverRequest.setAccessToken(std::move(jwtPharmacy));
    serverRequest.setQueryParameters({{"secret", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    // placeholder:
    //    <whenPrepared value="###WHENPREPARED###" />
    //    <whenHandedOver value="###WHENHANDEDOVER###" />
    {
        auto medicationDispenseXml = FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input_datetime_placeholder.xml");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###PRESCRIPTIONID###", correctId);
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###WHENPREPARED###", "2020");
        medicationDispenseXml = String::replaceAll(medicationDispenseXml, "###WHENHANDEDOVER###", "2020-12");
        serverRequest.setBody(std::move(medicationDispenseXml));
        EXPECT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        EXPECT_NO_THROW(handler.handleRequest(sessionContext));
    }
}

INSTANTIATE_TEST_SUITE_P(
    CloseTaskTestPInst, CloseTaskTestP,
    testing::Values(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_input1.xml",
                    std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_bundle.xml"));
