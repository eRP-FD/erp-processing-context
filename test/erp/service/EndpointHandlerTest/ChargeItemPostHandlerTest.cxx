/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "erp/service/chargeitem/ChargeItemPostHandler.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/FileHelper.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <gtest/gtest.h>

using OperationType = ResourceTemplates::ChargeItemOptions::OperationType;

namespace
{
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPostChargeItemHandler(std::optional<model::ChargeItem>& resultChargeItem, PcServiceContext& serviceContext,
                                JWT jwt, std::string body, const std::optional<model::PrescriptionId> prescriptionId,
                                const std::optional<std::string_view> secret, const HttpStatus expectedStatus,
                                const std::optional<std::string_view> expectedExcWhat = std::nullopt)
{
    ChargeItemPostHandler handler({});
    Header requestHeader{HttpMethod::POST,
                         "/ChargeItem/",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                         HttpStatus::Unknown};

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setBody(std::move(body));

    std::vector<std::pair<std::string, std::string>> parameters;
    if (prescriptionId.has_value())
    {
        parameters.emplace_back("task", prescriptionId->toString());
    }
    if (secret.has_value())
    {
        parameters.emplace_back("secret", std::string(secret.value()));
    }
    serverRequest.setQueryParameters(std::move(parameters));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler,
                                                                                    expectedStatus, expectedExcWhat));
    if (expectedStatus == HttpStatus::Created)
    {
        ASSERT_NO_THROW(resultChargeItem =
                            model::ChargeItem::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()));
        ASSERT_TRUE(resultChargeItem);
    }
}

}// namespace


class ChargeItemPostHandlerTest : public EndpointHandlerTest
{
    void SetUp() override
    {
    }
};

TEST_F(ChargeItemPostHandlerTest, PostChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50022);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};

    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(), CryptoHelper::cHpQesPrv(),
                                        ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId}),
                                        std::nullopt};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                                                 .prescriptionId = pkvTaskId,
                                                                 .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                                                 .operation = OperationType::Post});
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr.id()}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // successful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), referencedTask.secret(),
                                                       HttpStatus::Created));

    // GEMREQ-start A_22137#PostChargeItem
    EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
    EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);

    EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
    EXPECT_FALSE(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary)
                     .has_value());
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn());
    // GEMREQ-end A_22137#PostChargeItem

    // GEMREQ-start A_22134#PostChargeItem
    A_22134.test("KBV prescription bundle reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeaturePrescription)}.toUrn());
    // GEMREQ-end A_22134#PostChargeItem

    // GEMREQ-start A_22135-01#PostChargeItem
    A_22135_01.test("Receipt reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeatureReceipt)}.toUrn());
    // GEMREQ-end A_22135-01#PostChargeItem

    A_23704.test("Access code not present in returned chargeItem");
    EXPECT_FALSE(resultChargeItem->accessCode().has_value());

    // Error cases:

    A_22130.test("Check existence of 'task' parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       std::nullopt /*task id*/, referencedTask.secret(),
                                                       HttpStatus::BadRequest));

    {
        A_22131.test("Check existance of referenced task");
        // Id of task which does not exist:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 999999999);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::Conflict));
    }
    {
        A_22131.test("Check task in status 'completed'");
        // Id of task which is not completed:
        const auto pkvTaskId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::Conflict));
    }

    A_22132_02.test("Check that secret is provided as URL parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), std::nullopt /*secret()*/,
                                                       HttpStatus::Forbidden));
    A_22132_02.test("Check that secret from URL is equal to secret from referenced task");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), "invalidsecret",
                                                       HttpStatus::Forbidden));

    {
        A_22136_01.test("Validate input ChargeItem resource: Kvnr");
        model::Kvnr pkvKvnr{"X500000017", model::Kvnr::Type::pkv};
        const auto chargeItemXml =
            ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                              .prescriptionId = pkvTaskId,
                                              .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                              .operation = OperationType::Post});
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
    {
        A_22136_01.test("Validate input ChargeItem resource: TelematikId");
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("Invalid-TelematikId");
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, inputChargeItem.prescriptionId(),
                                                           referencedTask.secret(), HttpStatus::BadRequest));
    }
    {
        A_22136_01.test("Validate input ChargeItem resource: PrescriptionId");
        const auto pkvTaskId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
    {
        A_22133.test("Check consent");
        model::Kvnr pkvKvnr{"X500000017", model::Kvnr::Type::pkv};
        const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                              .prescriptionId = pkvTaskId,
                                              .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                              .operation = OperationType::Post});
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
}


TEST_F(ChargeItemPostHandlerTest, PostChargeItemNonQes)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkiPath = MockConfiguration::instance().getPathValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH);

    const auto nonQesSmcbCert =
        Certificate::fromPem(FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/nonQesSmcb.pem"));
    const auto nonQesSmcbPrivateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(
        SafeString{FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/nonQesSmcbPrivateKey.pem")});

    auto nonQesSmcbCertX509 = X509Certificate::createFromX509Pointer(nonQesSmcbCert.toX509().removeConst().get());

    const auto ocspResponseData = mServiceContext.getTslManager().getCertificateOcspResponse(
        TslMode::TSL, nonQesSmcbCertX509, {CertificateType::C_HCI_OSIG},
        TslTestHelper::getDefaultTestOcspCheckDescriptor());

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50022);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};

    CadesBesSignature cadesBesSignature{nonQesSmcbCert, nonQesSmcbPrivateKey,
                                        ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId}),
                                        std::nullopt, OcspHelper::stringToOcspResponse(ocspResponseData.response)};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                              .prescriptionId = pkvTaskId,
                                              .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                              .operation = OperationType::Post});
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr.id()}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // successful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), referencedTask.secret(),
                                                       HttpStatus::Created));

    // GEMREQ-start A_22137#PostChargeItemNonQes
    EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
    EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);

    EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
    EXPECT_FALSE(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary)
                     .has_value());
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn());
    // GEMREQ-end A_22137#PostChargeItemNonQes

    // GEMREQ-start A_22134#PostChargeItemNonQes
    A_22134.test("KBV prescription bundle reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeaturePrescription)}.toUrn());
    // GEMREQ-end A_22134#PostChargeItemNonQes

    // GEMREQ-start A_22135-01#PostChargeItemNonQes
    A_22135_01.test("Receipt reference set");
    EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle),
              Uuid{resultChargeItem->prescriptionId()->deriveUuid(model::uuidFeatureReceipt)}.toUrn());
    // GEMREQ-end A_22135-01#PostChargeItemNonQes

    // Error cases:

    A_22130.test("Check existence of 'task' parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       std::nullopt /*task id*/, referencedTask.secret(),
                                                       HttpStatus::BadRequest));

    {
        A_22131.test("Check existance of referenced task");
        // Id of task which does not exist:
        const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 999999999);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::Conflict));
    }
    {
        A_22131.test("Check task in status 'completed'");
        // Id of task which is not completed:
        const auto pkvTaskId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::Conflict));
    }

    A_22132_02.test("Check that secret is provided as URL parameter");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), std::nullopt,
                                                       HttpStatus::Forbidden));
    A_22132_02.test("Check that secret from URL is equal to secret from referenced task");
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), "invalidsecret",
                                                       HttpStatus::Forbidden));

    {
        A_22136_01.test("Validate input ChargeItem resource: Kvnr");
        const auto pkvKvnr = model::Kvnr{"X500000017", model::Kvnr::Type::pkv};
        const auto chargeItemXml =
            ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                              .prescriptionId = pkvTaskId,
                                              .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                              .operation = OperationType::Post});
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
    {
        A_22136_01.test("Validate input ChargeItem resource: TelematikId");
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("Invalid-TelematikId");
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, inputChargeItem.prescriptionId(),
                                                           referencedTask.secret(), HttpStatus::BadRequest));
    }
    {
        A_22136_01.test("Validate input ChargeItem resource: PrescriptionId");
        const auto pkvTaskId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
    {
        A_22133.test("Check consent");
        const auto pkvKvnr = model::Kvnr{"X500000017", model::Kvnr::Type::pkv};
        const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                                                 .prescriptionId = pkvTaskId,
                                                                 .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                                                 .operation = OperationType::Post});
        ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                           chargeItemXml, pkvTaskId, referencedTask.secret(),
                                                           HttpStatus::BadRequest));
    }
}

TEST_F(ChargeItemPostHandlerTest, PostChargeItemInvalidBundle)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};

    auto& resourceManager = ResourceManager::instance();
    auto dispenseBundleXml = resourceManager.getStringResource(
        std::string{TEST_DATA_DIR} + "/validation/xml/v_2023_07_01/dav/AbgabedatenBundle/"
                                     "Bundle_invalid_AbgabedatenBundle-missing-Bankverbindung.xml");
    dispenseBundleXml = regex_replace(dispenseBundleXml, std::regex{R"(<whenHandedOver value="[0-9-]+" />)"},
                                      "<whenHandedOver value=\"" + model::Timestamp::now().toGermanDate() + "\"/>");
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(), CryptoHelper::cHpQesPrv(), dispenseBundleXml,
                                        std::nullopt};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                                                 .prescriptionId = pkvTaskId,
                                                                 .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                                                 .operation = OperationType::Post});
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr.id()}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // expect failure with invalid bundle
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), referencedTask.secret(),
                                                       HttpStatus::BadRequest, "FHIR-Validation error"));
}

TEST_F(ChargeItemPostHandlerTest, PostChargeItemInvalidBundleVersion)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string davDispenseItemProfile{profile(model::ProfileType::DAV_PKV_PR_ERP_AbgabedatenBundle).value()};
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};
    auto dispenseBundleXml = ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId});
    boost::replace_all(dispenseBundleXml, davDispenseItemProfile + '|' + to_string(ResourceTemplates::Versions::DAV_PKV_current()),
        davDispenseItemProfile + "|0.1");
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(), CryptoHelper::cHpQesPrv(), dispenseBundleXml,
                                        std::nullopt};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml({.kvnr = pkvKvnr,
                                                                 .prescriptionId = pkvTaskId,
                                                                 .dispenseBundleBase64 = cadesBesSignature.getBase64(),
                                                                 .operation = OperationType::Post});
    const auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr.id()}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // expect failure with invalid bundle
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, chargeItemXml,
                                                       inputChargeItem.prescriptionId(), referencedTask.secret(),
                                                       HttpStatus::BadRequest, "parsing / validation error"));
}

TEST_F(ChargeItemPostHandlerTest, PostChargeItemInvalidChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000056";

    auto& resourceManager = ResourceManager::instance();
    const auto chargeItemXml = resourceManager.getStringResource(
        std::string{TEST_DATA_DIR} + "/validation/xml/pkv/chargeItem/ChargeItem_invalid_wrongEnterer.xml");
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setPrescriptionId(pkvTaskId);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // successful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, inputChargeItem.serializeToXmlString(),
        inputChargeItem.prescriptionId(), referencedTask.secret(), HttpStatus::BadRequest, "FHIR-Validation error"));
}

TEST_F(ChargeItemPostHandlerTest, PostChargeItemInvalidChargeItemVersion)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const char* const pkvKvnr = "X500000056";

    auto& resourceManager = ResourceManager::instance();
    const auto chargeItemXml = resourceManager.getStringResource(
        std::string{TEST_DATA_DIR} + "/validation/xml/pkv/chargeItem/ChargeItem_invalid_wrongVersion.xml");
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setPrescriptionId(pkvTaskId);

    const auto referencedTask = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId, .kvnr = pkvKvnr}));

    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    // successful retrieval:
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPostChargeItemHandler(
        resultChargeItem, mServiceContext, jwtPharmacy, inputChargeItem.serializeToXmlString(),
        inputChargeItem.prescriptionId(), referencedTask.secret(), HttpStatus::BadRequest, "parsing / validation error"));
}

TEST_F(ChargeItemPostHandlerTest, PostNonPkvFails)
{
    ChargeItemPostHandler handler{{}};
    auto serviceContext = StaticData::makePcServiceContext();
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    Header requestHeader{HttpMethod::POST, "/chargeitem/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setQueryParameters({{"task", prescriptionId.toString()}});
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext(serviceContext, serverRequest, serverResponse, accessLog);
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try
    {
        handler.handleRequest(sessionContext);
        ADD_FAILURE() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
        EXPECT_STREQ(ex.what(), "Referenced task is not of type PKV");
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}
