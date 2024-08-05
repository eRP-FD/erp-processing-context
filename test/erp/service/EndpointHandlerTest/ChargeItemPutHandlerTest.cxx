/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "erp/util/Base64.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"

#include <gtest/gtest.h>
#include <erp/util/Demangle.hxx>

using OperationType = ResourceTemplates::ChargeItemOptions::OperationType;

class ChargeItemPutHandlerTest : public EndpointHandlerTest
{
    void SetUp() override
    {
    }

public:
    static void resetChargeItem(SessionContext& sessionContext, const model::PrescriptionId prescriptionId)
    {
        auto* database = sessionContext.database();
        auto [existingChargeInformation, blobId, salt] = database->retrieveChargeInformationForUpdate(prescriptionId);
        existingChargeInformation.chargeItem.setAccessCode(MockDatabase::mockAccessCode);
        database->updateChargeInformation(existingChargeInformation, blobId, salt);
        database->commitTransaction();
    }
};


namespace
{

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkPutChargeItemHandler(std::optional<model::ChargeItem>& resultChargeItem, PcServiceContext& serviceContext,
                               JWT jwt, const ContentMimeType& contentType, const model::ChargeItem& chargeItem,
                               const std::optional<model::PrescriptionId>& id, const HttpStatus expectedStatus,
                               const std::optional<std::string_view> expectedExcWhat = std::nullopt)
{
    const auto idStr = id.has_value() ? id->toString() : "";
    ChargeItemPutHandler handler({});
    Header requestHeader{HttpMethod::PUT,
                         "/ChargeItem/" + idStr,
                         0,
                         {{Header::ContentType, contentType}},
                         HttpStatus::Unknown};

    const auto accessCode = chargeItem.accessCode();
    if (accessCode.has_value())
    {
        requestHeader.addHeaderField(Header::XAccessCode, accessCode->data());
    }

    std::string body = (contentType.getMimeType() == MimeType::fhirJson) ? chargeItem.serializeToJsonString()
                                                                         : chargeItem.serializeToXmlString();

    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(std::move(jwt));
    serverRequest.setBody(std::move(body));

    if (id.has_value())
    {
        serverRequest.setPathParameters({"id"}, {idStr});
    }

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_FATAL_FAILURE(EndpointHandlerTest::callHandlerWithResponseStatusCheck(sessionContext, handler,
                                                                                    expectedStatus, expectedExcWhat));
    if (id.has_value())
    {
        ChargeItemPutHandlerTest::resetChargeItem(sessionContext, id.value());
    }
    if (expectedStatus == HttpStatus::OK)
    {
        if (contentType.getMimeType() == MimeType::fhirJson)
        {
            ASSERT_NO_THROW(resultChargeItem =
                                model::ChargeItem::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));
        }
        else
        {
            ASSERT_NO_THROW(resultChargeItem =
                                model::ChargeItem::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()));
        }
        ASSERT_TRUE(resultChargeItem);
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkUnchangedChargeItemFieldsCommon(PcServiceContext& serviceContext, const JWT& jwt,
                                          const ContentMimeType& contentType, const std::string_view& chargeItemXml,
                                          const model::PrescriptionId& pkvTaskId)
{
    std::optional<model::ChargeItem> resultChargeItem;
    const auto errTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 99999);
    {
        // id
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setId(errTaskId);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // identifier
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setPrescriptionId(errTaskId);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Telematik id
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setEntererTelematikId("some-other-telematik-id");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Kvnr
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setSubjectKvnr("some-other-kvnr");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // entered date
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.setEnteredDate(model::Timestamp::now());
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Receipt reference
        auto chargeItemErrorRef = String::replaceAll(
            std::string{chargeItemXml}, Uuid{pkvTaskId.deriveUuid(model::uuidFeatureReceipt)}.toUrn(), "error-ref");
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemErrorRef);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Prescription reference
        auto chargeItemErrorRef =
            String::replaceAll(std::string{chargeItemXml},
                               Uuid{pkvTaskId.deriveUuid(model::uuidFeaturePrescription)}.toUrn(), "error-ref");
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemErrorRef);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
    {
        // Dispense reference
        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        errChargeItem.deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, serviceContext, jwt, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::BadRequest));
    }
}

}// namespace



TEST_F(ChargeItemPutHandlerTest, PutChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};

    const auto newDispenseBundleXml = ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId});
    auto newDispenseBundle = model::Bundle::fromXmlNoValidation(newDispenseBundleXml);
    // set new ID to check update
    newDispenseBundle.setId(Uuid());
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(), CryptoHelper::cHpQesPrv(),
                                        newDispenseBundle.serializeToXmlString(), std::nullopt};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml(
        {.kvnr = pkvKvnr,
         .prescriptionId = pkvTaskId,
         .dispenseBundleBase64 = cadesBesSignature.getBase64(),
         .operation = OperationType::Put});
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    {
        // successful update:
        // GEMREQ-start A_22148
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                          inputChargeItem, pkvTaskId, HttpStatus::OK));
        EXPECT_EQ(resultChargeItem->id(), pkvTaskId);
        EXPECT_EQ(resultChargeItem->prescriptionId(), pkvTaskId);
        EXPECT_FALSE(resultChargeItem->containedBinary().has_value());
        EXPECT_FALSE(
            resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBinary)
                .has_value());
        EXPECT_EQ(resultChargeItem->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
                  Uuid{pkvTaskId.deriveUuid(model::uuidFeatureDispenseItem)}.toUrn());
        A_23624.test("Charge item does not contain access code");
        EXPECT_FALSE(resultChargeItem->accessCode().has_value());
        // GEMREQ-end A_22148
    }
    {
        // Error: check match of telematik id from access token and ChargeItem
        A_22146.test("Pharmacy: validation of TelematikId");
        const auto errJwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke("wrong-telematik-id");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, errJwtPharmacy,
                                                          contentType, inputChargeItem, pkvTaskId,
                                                          HttpStatus::Forbidden));
    }
    {
        // check with invalid access code:
        A_22616_03.test("Access code check");
        inputChargeItem.setAccessCode("invalid-access-code");
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                          inputChargeItem, pkvTaskId, HttpStatus::Forbidden));
    }
    {
        // Error cases: Values other than dispense item changed in new ChargeItem
        A_22152.test("Check that ChargeItem fields other than dispense item are unchanged");
        ASSERT_NO_FATAL_FAILURE(
            checkUnchangedChargeItemFieldsCommon(mServiceContext, jwtPharmacy, contentType, chargeItemXml, pkvTaskId));
        {
            // Pharmacy: Marking
            const auto errChargeItemXml = String::replaceAll(chargeItemXml, "false", "true");
            auto errChargeItem = model::ChargeItem::fromXmlNoValidation(errChargeItemXml);
            errChargeItem.setAccessCode(MockDatabase::mockAccessCode);
            ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                              contentType, errChargeItem, pkvTaskId,
                                                              HttpStatus::BadRequest));

            errChargeItem.deleteMarkingFlag();
            ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy,
                                                              contentType, errChargeItem, pkvTaskId,
                                                              HttpStatus::BadRequest));
        }
    }
    {
        A_22215.test("Check consent");
        const auto pkvKvnr = model::Kvnr{"X500000017"};// no consent exists for this kvnr
        const auto errChargeItemXml = ResourceTemplates::chargeItemXml(
            {.kvnr = pkvKvnr,
             .prescriptionId = pkvTaskId,
             .dispenseBundleBase64 = cadesBesSignature.getBase64(),
             .operation = OperationType::Put});

        auto errChargeItem = model::ChargeItem::fromXmlNoValidation(errChargeItemXml);
        errChargeItem.setAccessCode(MockDatabase::mockAccessCode);
        ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                          errChargeItem, pkvTaskId, HttpStatus::Forbidden));
    }
}

TEST_F(ChargeItemPutHandlerTest, PutChargeItemInvalidChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};
    auto chargeItemXml = ResourceTemplates::chargeItemXml(
        {.kvnr = pkvKvnr,
         .prescriptionId = pkvTaskId,
         .dispenseBundleBase64 = "aGFsbG8K",
         .operation = OperationType::Put});

    chargeItemXml = String::replaceAll(chargeItemXml, "kvid-10", "kvid11");
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    // expected reject
    ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                      inputChargeItem, pkvTaskId, HttpStatus::BadRequest,
                                                      "FHIR-Validation error"));
}

TEST_F(ChargeItemPutHandlerTest,
       PutChargeItemInvalidChargeItemVersion)//NOLINT(readability-function-cognitive-complexity)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);

    auto& resourceManager = ResourceManager::instance();
    const auto chargeItemXml = resourceManager.getStringResource(
        std::string{TEST_DATA_DIR} + "/validation/xml/pkv/chargeItem/ChargeItem_invalid_wrongVersion.xml");

    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    // expected reject
    ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                      inputChargeItem, pkvTaskId, HttpStatus::BadRequest,
                                                      "FHIR-Validation error"));
}


TEST_F(ChargeItemPutHandlerTest, PutNonPkvFails)
{
    ChargeItemPutHandler handler{{}};
    auto serviceContext = StaticData::makePcServiceContext();
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    Header requestHeader{HttpMethod::POST, "/chargeitem/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setPathParameters({"id"},  {prescriptionId.toString()});
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
        EXPECT_STREQ(ex.what(), "Attempt to access charge item for non-PKV Prescription.");
    }
    catch (const std::exception& ex)
    {
        ADD_FAILURE() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}
