/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Demangle.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test_config.h"

#include <gtest/gtest.h>

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
        requestHeader.addHeaderField(Header::XAccessCode, std::string{*accessCode});
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
    const char* const pkvKvnr = "X500000056";
    auto chargeItemXml = ResourceTemplates::chargeItemXml({
        .kvnr = model::Kvnr{pkvKvnr, model::Kvnr::Type::pkv},
        .prescriptionId = pkvTaskId,
        .dispenseBundleBase64 = "MmEzN2MyZDItNTFjNy00YTU3LTk3MGQtMTFmMWI4MjA0YmYyCg==",
        .operation = OperationType::Put,
    });
    chargeItemXml = std::regex_replace(chargeItemXml, std::regex{R"(GEM_ERPCHRG_PR_ChargeItem\|[^"]+)"},
                                       "GEM_ERPCHRG_PR_ChargeItem|0.1");

    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    // expected reject
    ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                      inputChargeItem, pkvTaskId, HttpStatus::BadRequest,
                                                      "parsing / validation error"));
}


TEST_F(ChargeItemPutHandlerTest, PutChargeItem_WithProfileVersion)//NOLINT(readability-function-cognitive-complexity)
{
    static const rapidjson::Pointer binaryProfilePtr{
        model::resource::ElementName::path(model::resource::elements::contained, 0, model::resource::elements::meta,
                                           model::resource::elements::profile, 0)};
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto pkvKvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv};

    const auto newDispenseBundleXml = ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId});
    auto newDispenseBundle = model::Bundle::fromXmlNoValidation(newDispenseBundleXml);
    // set new ID to check update
    newDispenseBundle.setId(Uuid());
    CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(), CryptoHelper::cHpQesPrv(),
                                        newDispenseBundle.serializeToXmlString(), std::nullopt};
    const auto chargeItemXml = ResourceTemplates::chargeItemXml({
        .kvnr = pkvKvnr,
        .prescriptionId = pkvTaskId,
        .dispenseBundleBase64 = cadesBesSignature.getBase64(),
        .operation = OperationType::Put,
    });
    auto chargeItemJson = Fhir::instance().converter().xmlStringToJson(chargeItemXml);
    chargeItemJson.setValue(binaryProfilePtr, "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Binary|" +
                                                  ResourceTemplates::Versions::GEM_ERP_current().renderVersion());
    auto inputChargeItem = model::ChargeItem::fromJson(std::move(chargeItemJson));

    inputChargeItem.setAccessCode(MockDatabase::mockAccessCode);
    const auto jwtPharmacy =
        JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId().value()));

    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;
    ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                      inputChargeItem, pkvTaskId, HttpStatus::OK));
}

TEST_F(ChargeItemPutHandlerTest, PutChargeItemErp23745)
{
    auto timestamp = model::Timestamp::fromGermanDate("2024-10-29");
    auto prescriptionId = model::PrescriptionId::fromString("200.000.000.050.000.33");
    auto dispenseItem = ResourceTemplates::davDispenseItemXml({
        .davPkvVersion = ResourceTemplates::Versions::DAV_PKV_1_2,
        .prescriptionId = prescriptionId,
        .whenHandenOver = timestamp,
    });
    auto signedItem = CryptoHelper::toCadesBesSignature(
        dispenseItem, "test/generated_pki/sub_ca1_ec/certificates/apotheker/apotheker.pem", timestamp);

    auto chargeItemStr = ResourceTemplates::chargeItemXml({
        .kvnr = model::Kvnr{"X500000056", model::Kvnr::Type::pkv},
        .prescriptionId = prescriptionId,
        .dispenseBundleBase64 = std::move(signedItem),
        .operation = ResourceTemplates::ChargeItemOptions::OperationType::Put,
    });
    auto inputChargeItem = model::ChargeItem::fromXml(chargeItemStr, *StaticData::getXmlValidator());
    inputChargeItem.setAccessCode("b79e5bca8b072113f08c43ce22aa1dded4db61ef21571b37911b6dfc852004f6");
    inputChargeItem.deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(std::string("606358757"));

    auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const ContentMimeType contentType = ContentMimeType::fhirXmlUtf8;
    std::optional<model::ChargeItem> resultChargeItem;

    // Expect BadRequest supportingInformation/Prescription is missing, was previously SEGV
    ASSERT_NO_FATAL_FAILURE(checkPutChargeItemHandler(resultChargeItem, mServiceContext, jwtPharmacy, contentType,
                                                      inputChargeItem, pkvTaskId, HttpStatus::BadRequest));
}
