/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "erp/model/Communication.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
class CommunicationPayloadTest : public ::testing::Test
{
    int globalOffset = Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS);
    EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V1_VALID_UNTIL",
                                      model::Timestamp(model::Timestamp::GermanTimezone,
                                                       model::Timestamp::now().localDay() - date::days{globalOffset})
                                          .toGermanDate()};
};

using namespace model;
namespace
{

enum ExpectedOutcome
{
    success,
    failure,
};


void testVerifyPayload(model::Communication::MessageType messageType, std::string_view payload,
                       ExpectedOutcome expectedOutcome, std::optional<std::string_view> expectedMessage = std::nullopt,
                       bool withDiagnostics = true,
                       model::PrescriptionId prescriptionId =
                           PrescriptionId::fromDatabaseId(PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4711))
{
    auto builder = CommunicationJsonStringBuilder(messageType);
    builder.setPayload(std::string{payload}).setPrescriptionId(prescriptionId.toString());
    switch (messageType)
    {
        using enum Communication::MessageType;
        case DispReq:
            builder.setAbout("#5e164c7d-8dc4-4e46-9b25-163f05bd203f");
            [[fallthrough]];
        case ChargChangeReq:
            builder.setSender(ActorRole::Insurant, "X555000111")
                .setRecipient(ActorRole::Pharmacists, "SMC-B-0815-4711");
            break;
        case Representative:
            builder.setSender(ActorRole::Insurant, "X666000111").setRecipient(ActorRole::Insurant, "X555000111");
            break;
        case Reply:
        case ChargChangeReply:
        case DiGA:
            builder.setSender(ActorRole::Pharmacists, "SMC-B-0815-4711")
                .setRecipient(ActorRole::Insurant, "X555000111");
            break;
    }

    std::string body = builder.createJsonString();
    auto jsonValidator = StaticData::getJsonValidator();
    const auto comm = ResourceFactory<Communication>::fromJson(
                          std::move(body), *jsonValidator, {.genericValidationMode = GenericValidationMode::disable})
                          .getValidated(Communication::messageTypeToProfileType(messageType));
    try
    {
        comm.verifyPayload(*jsonValidator);
        comm.verifySupplyOptionsType(prescriptionId.type());
    }
    catch (const ErpException& e)
    {
        ASSERT_EQ(expectedOutcome, ExpectedOutcome::failure) << e.what() << e.diagnostics().value_or("");
        if (expectedMessage.has_value())
        {
            ASSERT_TRUE(String::starts_with(std::string_view{e.what()}, *expectedMessage)) << e.what();
            ASSERT_EQ(e.diagnostics().has_value(), withDiagnostics) << body;
        }
        return;
    }
    catch (const std::exception& e)
    {
        ASSERT_EQ(expectedOutcome, ExpectedOutcome::failure) << e.what();
        if (expectedMessage.has_value())
        {
            ASSERT_TRUE(String::starts_with(std::string_view{e.what()}, *expectedMessage)) << e.what();
        }
        return;
    }

    ASSERT_EQ(expectedOutcome, ExpectedOutcome::success);
}

}

TEST_F(CommunicationPayloadTest, verifyPayloadV1DispReq)
{
    const auto msgType = Communication::MessageType::DispReq;
    A_23878_01.test("Validierung DispReq payload");

    // simple success cases
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"shipment","name": "Hans Peter Willich","address":["Hausnummer 1","3.OG","12345 Berlin"],"hint":"bla","phone":"004916094858168"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"delivery","name": "Hans Peter Willich","address":["Hausnummer 1","3.OG","12345 Berlin"],"hint":"bla","phone":"004916094858168"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }

    // failures
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"invalid"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({"version":2,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid payload version: 2"));
    }
    {
        std::string_view payload = R"({})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "version must be 'integer'"));
    }
    {
        std::string_view payload = R"(not-a-json)";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid JSON in payload.contentString", false));
    }
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise","extraElement":"1"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"shipment"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "for flowType 166 only onPremise and delivery are allowed", false,
                                                  PrescriptionId::fromDatabaseId(PrescriptionType::tRezept, 4800)));
    }
    // failures with too large field lengths
    {
        std::string longName(101, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","name":")" + longName + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","name":")" + longName.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longHint(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","hint":")" + longHint + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","hint":")" + longHint.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longPhone(33, '0');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","phone":")" + longPhone + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","phone":")" + longPhone.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string longAddress(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","address":[")" + longAddress + "\",\"" +
                              longAddress + R"("]})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","address":[")" + longAddress.substr(1) + "\",\"" +
                  longAddress.substr(1) + R"("]})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
}

TEST_F(CommunicationPayloadTest, verifyPayloadV1Reply)
{
    A_23879.test("Validierung DispReq payload");
    const auto msgType = Communication::MessageType::Reply;

    // simple success cases
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"onPremise","info_text":"Hallo","url":"https://gematik.de","pickUpCodeHR":"12345678","pickUpCodeDMC":"123459789"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"delivery","info_text":"Hallo der Herr","url":"https://gematik.de"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string_view payload =
            R"({"version":1,"supplyOptionsType":"shipment","info_text":"Hallo der Herr","url":"https://www.example.com/path/to/file.txt?userid=1001&pages=3&results=full#page1"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    // failures
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"invalid"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    {
        std::string_view payload = R"({"version":2,"supplyOptionsType":"onPremise"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid payload version: 2"));
    }
    {
        std::string_view payload = R"({})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "version must be 'integer'"));
    }
    {
        std::string_view payload = R"(not-a-json)";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid JSON in payload.contentString"));
    }
    {
        std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise","extraElement":"1"})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
    }
    // failures with too large field lengths
    {
        std::string longInfo(501, 'A');
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","info_text":")" + longInfo + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","info_text":")" + longInfo.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string url{"https://gematik.de/"};
        url.append(std::string(501 - url.size(), 'A'));
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string pickUpCodeHR(9, 'A');
        std::string payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeHR":")" + pickUpCodeHR + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload = R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeHR":")" + pickUpCodeHR.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    {
        std::string pickUpCodeDMC(129, 'A');
        std::string payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeDMC":")" + pickUpCodeDMC + R"("})";
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(msgType, payload, failure, "Invalid payload: does not conform to expected JSON schema:"));
        payload =
            R"({"version":1,"supplyOptionsType":"onPremise","pickUpCodeDMC":")" + pickUpCodeDMC.substr(1) + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
    // requirement to have supplyOptionsType = "onPremise" when pickUpCodeDMC or pickUpCodeHR are set
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"shipment","pickUpCodeDMC":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"delivery","pickUpCodeDMC":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"shipment","pickUpCodeHR":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string payload = R"({"version":1,"supplyOptionsType":"delivery","pickUpCodeHR":"abcd"})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure,
                                                  "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'"));
    }
    {
        std::string url{"something"};
        std::string payload = R"({"version":1,"supplyOptionsType":"onPremise","url":")" + url + R"("})";
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, failure, "Invalid payload: URL not valid."));
    }
}


TEST_F(CommunicationPayloadTest, verifyPayloadNoJson)
{
    using enum model::Communication::MessageType;
    std::string_view payload = "not-a-json";
    std::vector<model::Communication::MessageType> messageTypes;
    // 2023 profiles must all, without DispReq & Reply
    messageTypes = {Representative, ChargChangeReply, ChargChangeReq};

    for (const auto& msgType : messageTypes)
    {
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(msgType, payload, success));
    }
}

TEST_F(CommunicationPayloadTest, validityPayloadV1)
{
    auto globalOffset = Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS);
    const auto today = model::Timestamp(model::Timestamp::GermanTimezone,
                                        model::Timestamp::now().localDay() - date::days{globalOffset});
    const std::string_view payload = R"({"version":1,"supplyOptionsType":"onPremise"})";
    {
        const EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V1_VALID_UNTIL",
                                                today.toGermanDate()};
        std::cout << "offset: " << globalOffset << " today: " << today.toGermanDate()
                  << " validUntil: " << Configuration::instance().communicationPayloadV1ValidUntil().toGermanDate() << std::endl;
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(Communication::MessageType::DispReq, payload, success));
    }
    {
        const EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V1_VALID_UNTIL",
                                                (today - date::days{1}).toGermanDate()};
        const auto validUntil = Configuration::instance().communicationPayloadV1ValidUntil();
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(Communication::MessageType::DispReq, payload, failure,
                              "Communication payload version 1 was valid until " + validUntil.toGermanDate())) << validUntil.toGermanDate();
    }
}
TEST_F(CommunicationPayloadTest, validityPayloadV3)
{
    auto globalOffset = Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS);
    const auto today = model::Timestamp(model::Timestamp::GermanTimezone,
                                        model::Timestamp::now().localDay() - date::days{globalOffset});
    const std::string_view payload =
        R"({ "version": 3, "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "text", "phone": "+49555555555", "text": "Gibt es noch Traubenzucker?" })";
    {
        const EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V3_VALID_FROM",
                                                today.toGermanDate()};
        EXPECT_NO_FATAL_FAILURE(testVerifyPayload(Communication::MessageType::DispReq, payload, success));
    }

    {
        const EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V3_VALID_FROM",
                                                (today+date::days{1}).toGermanDate()};
        const auto validFrom = Configuration::instance().communicationPayloadV3ValidFrom();
        EXPECT_NO_FATAL_FAILURE(
            testVerifyPayload(Communication::MessageType::DispReq, payload, failure,
                              "Communication payload version 3 will be valid from " + validFrom.toGermanDate()));
    }
}

struct PayloadTextParams {
    Communication::MessageType messageType;
    std::string_view payload;
    ExpectedOutcome expectedOutcome;
    std::optional<std::string_view> expectedMessage;
    std::string testName = "valid";
};

class CommunicationPayloadV3TestP : public CommunicationPayloadTest,
                                    public testing::WithParamInterface<PayloadTextParams>
{
public:
    static std::string name(testing::TestParamInfo<ParamType> p)
    {
        return p.param.testName;
    }
    int globalOffset = Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS);
    EnvironmentVariableGuard envGuard{"ERP_SERVICE_COMMUNICATION_PAYLOAD_V3_VALID_FROM",
                                      model::Timestamp(model::Timestamp::GermanTimezone,
                                                       model::Timestamp::now().localDay() - date::days{globalOffset})
                                          .toGermanDate()};
};

TEST_P(CommunicationPayloadV3TestP, verifyPayloadV3)
{
    A_23879_01.test("verify json payload V3 of Reply");
    A_23878_02.test("verify json payload V3 of DispReq");
    testVerifyPayload(GetParam().messageType, GetParam().payload, GetParam().expectedOutcome,
                      GetParam().expectedMessage);
}

INSTANTIATE_TEST_SUITE_P(
    dispenseRequest_valid, CommunicationPayloadV3TestP,
    testing::Values(
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID": "ABCD-EFGH-IJKL-MNOP", "version": 3, "communicationType": "order", "supplyOptionsType": "delivery", "phone": "+49555555555", "text": "Need it now!", "firstname": "Max", "lastname": "Mustermann", "address": "Musterstraße 555", "postcode": "55555", "city": "Musterhause", "country": "DE"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "delivery"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID": "ABCD-EFGH-IJKL-MNOP", "version": 3, "communicationType": "order", "supplyOptionsType": "shipment", "phone": "+49555555555", "text": "Need it now!", "firstname": "Max", "lastname": "Mustermann", "address": "Musterstraße 555", "postcode": "55555", "city": "Musterhause", "country": "DE"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "shipment"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID": "ABCD-EFGH-IJKL-MNOP", "version": 3, "communicationType": "order", "supplyOptionsType": "onPremise", "phone": "+49555555555", "text": "Need it now!"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "reservation"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"version": 3, "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "text", "phone": "+49555555555", "text": "Gibt es noch Traubenzucker?"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "message"}),
    &CommunicationPayloadV3TestP::name);

INSTANTIATE_TEST_SUITE_P(
    reply_valid, CommunicationPayloadV3TestP,
    testing::Values(
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Vielen Dank für Ihre Bestellung!", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "text"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "text"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "link", "url": "https://example.com", "text": "Hier finden sie Ihren Warenkorb."})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "link"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "reservationStatus", "readyForCollection": "immediately"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "reservationstatus_01"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "reservationStatus", "readyForCollection": "nextDay"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "reservationstatus_02"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "pickupCodeHR", "pickupCodeHR": "0815"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "pickupCodeHR"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "pickupCodeDMC", "pickupCodeDMC": "MACHINE_READABLE_CONTENT_OR_STRUCTURED_DATA"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "pickupCodeDMC"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "deliveryStatus", "deliveryStatus": "incident"})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "deliveryStatus_01_failed"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "deliveryStatus", "deliveryStatus": "inTransport", "inTransportPosition": {"long": 13.387595793605172, "lat": 52.522529939635795}, "inTransportETA": {"from": 1735736400, "to": 1735741800}})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "deliveryStatus_02_inTransition"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "paymentInfo", "totalAmount": 12530, "paymentMethods": [{"method": "cash"}, {"method": "bankaccount", "url": "https://my.payment.provider.de/pay/<payment_transaction_id>"}, {"method": "paypal", "url": "https://paypal.me/<some_account>"}]})_",
            .expectedOutcome = success,
            .expectedMessage = std::nullopt,
            .testName = "paymentInfo"}// PayloadTextParams{
                                      //     .messageType = Communication::MessageType::Reply,
                                      //     .payload =
        //         R"_({"version": 3, "text": "Some Text to state your request", "transactionID": "ABCD-EFGH-IJKL-MNOP", "communicationType": "availabilityResponse", "supplyOptionsType": "reservation", "availabilityResponse": "immediately"})_",
        //     .expectedOutcome = success,
        //     .expectedMessage = std::nullopt,
        //     .testName = "reservation_01_immediately"}
        ),
    &CommunicationPayloadV3TestP::name);

std::string makePayloadWithField(std::string_view prefix, const std::string& value, std::string_view suffix)
{
    return std::string(prefix) + value + std::string(suffix);
}

const std::string kLongText801 = std::string(801, 'A');
const std::string kLongHint101 = std::string(101, 'H');
const std::string kLongFirstname46 = std::string(46, 'F');
const std::string kLongLastname46 = std::string(46, 'L');
const std::string kLongAddress101 = std::string(101, 'A');
const std::string kLongPostcode11 = std::string(11, '1');
const std::string kLongCity101 = std::string(101, 'C');
const std::string kLongCountry4 = std::string(4, 'D');
const std::string kLongTransactionId51 = std::string(51, 'T');
const std::string kLongPickupCode9 = std::string(9, 'P');
const std::string kShortPickupCodeDmc7 = std::string(7, 'D');
const std::string kLongPickupCodeDmc2001 = std::string(2001, 'D');
const std::string kLongPhone33 = std::string("+") + std::string(32, '1');
const std::string kLongUrl501 = [] {
    std::string url = "https://example.com/";
    url.append(501 - url.size(), 'U');
    return url;
}();

const std::string kDispReqFirstnameTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":")_",
    kLongFirstname46,
    R"_(","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_");
const std::string kDispReqLastnameTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":")_",
    kLongLastname46, R"_(","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_");
const std::string kDispReqAddressTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":")_",
    kLongAddress101, R"_(","postcode":"12345","city":"Berlin","country":"DE"})_");
const std::string kDispReqPostcodeTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":")_",
    kLongPostcode11, R"_(","city":"Berlin","country":"DE"})_");
const std::string kDispReqCityTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":")_",
    kLongCity101, R"_(","country":"DE"})_");
const std::string kDispReqCountryTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":")_",
    kLongCountry4, R"_("})_");
const std::string kDispReqHintTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"reservation","phone":"+49555555555","hint":")_",
    kLongHint101, R"_("})_");
const std::string kDispReqTextTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"text","phone":"+49555555555","text":")_",
    kLongText801, R"_("})_");
const std::string kDispReqPhoneTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"reservation","phone":")_",
    kLongPhone33, R"_("})_");
const std::string kDispReqTransactionIdTooLong = makePayloadWithField(
    R"_({"transactionID":")_", kLongTransactionId51,
    R"_(","version":3,"communicationType":"order","supplyOptionsType":"reservation","phone":"+49555555555"})_");

const std::string kReplyTransactionIdTooLong = makePayloadWithField(
    R"_({"transactionID":")_", kLongTransactionId51, R"_(","version":3,"communicationType":"text","text":"Hi"})_");
const std::string kReplyTextTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"text","text":")_", kLongText801, R"_("})_");
const std::string kReplyUrlTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"link","text":"See this","url":")_", kLongUrl501,
    R"_("})_");
const std::string kReplyPickupCodeTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"pickupCodeHR","pickupCodeHR":")_", kLongPickupCode9,
    R"_("})_");
const std::string kReplyPickupCodeDmcTooShort = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"pickupCodeDMC","pickupCodeDMC":")_",
    kShortPickupCodeDmc7, R"_("})_");
const std::string kReplyPickupCodeDmcTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"pickupCodeDMC","pickupCodeDMC":")_",
    kLongPickupCodeDmc2001, R"_("})_");
const std::string kReplyPaymentMethodUrlTooLong = makePayloadWithField(
    R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","totalAmount":1200,"paymentMethods":[{"method":"paypal","url":")_",
    kLongUrl501, R"_("}]})_");

INSTANTIATE_TEST_SUITE_P(
    dispenseRequest_invalid, CommunicationPayloadV3TestP,
    testing::Values(
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "version must be 'integer'",
            .testName = "missingVersion"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":"3","communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "version must be 'integer'",
            .testName = "versionString"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":4,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload version: 4",
            .testName = "versionConst"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"invalid","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "communicationTypeInvalid"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingCommunicationType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"invalid","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "supplyOptionsTypeInvalid"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingFirstname"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqFirstnameTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "firstnameTooLong"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqLastnameTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "lastnameTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"ab","postcode":"12345","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "addressTooShort"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqAddressTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "addressTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12","city":"Berlin","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "postcodeTooShort"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqPostcodeTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "postcodeTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"A","country":"DE"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "cityTooShort"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqCityTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "cityTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"D"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "countryTooShort"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqCountryTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "countryTooLong"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqHintTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "hintTooLong"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqTextTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "textTooLong"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqPhoneTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "phoneTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"reservation","phone":"12345"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "phonePattern"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"reservation","phone":"+49555555555","mail":"not-an-email"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "mailPattern"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"version":3,"communicationType":"text","phone":"+49555555555","text":"Gibt es noch Traubenzucker?"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingTransactionId"},
        PayloadTextParams{.messageType = Communication::MessageType::DispReq,
                          .payload = kDispReqTransactionIdTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "transactionIdTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::DispReq,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"order","supplyOptionsType":"delivery","phone":"+49555555555","firstname":"Max","lastname":"Mustermann","address":"Main Street 1","postcode":"12345","city":"Berlin","country":"DE","extra":"nope"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "additionalProperty"}),
    &CommunicationPayloadV3TestP::name);

INSTANTIATE_TEST_SUITE_P(
    reply_invalid, CommunicationPayloadV3TestP,
    testing::Values(
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = R"_({"transactionID":"ABCD-EFGH","communicationType":"text","text":"Hi"})_",
                          .expectedOutcome = failure,
                          .expectedMessage = "version must be 'integer'",
                          .testName = "missingVersion"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":"3","communicationType":"text","text":"Hi"})_",
            .expectedOutcome = failure,
            .expectedMessage = "version must be 'integer'",
            .testName = "versionString"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":4,"communicationType":"text","text":"Hi"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload version: 4",
            .testName = "versionConst"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"text":"Hi"})_",
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "missingCommunicationType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"invalid","text":"Hi"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "communicationTypeInvalid"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = R"_({"version":3,"communicationType":"text","text":"Hi"})_",
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "missingTransactionId"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyTransactionIdTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "transactionIdTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"text","text":"Hi","extra":"nope"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "additionalProperty"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyTextTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "textTooLong"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyUrlTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "urlTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"link","text":"See this"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingUrl"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"reservationStatus","readyForCollection":"later"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "readyForCollectionInvalid"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"reservationStatus"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingReadyForCollection"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"pickUpCodeHR"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingPickupCode"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyPickupCodeTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "pickupCodeTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"pickUpCodeHR","pickupCodeHR":"ABC-12"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "pickupCodePattern"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyPickupCodeDmcTooShort,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "pickupCodeDmcTooShort"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyPickupCodeDmcTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "pickupCodeDmcTooLong"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload = R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"deliveryStatus"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingDeliveryStatus"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"deliveryStatus","deliveryStatus":"lost"})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "deliveryStatusInvalid"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"deliveryStatus","deliveryStatus":"inTransport","inTransportPosition":{"lat":"52.1","long":13.2}})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "inTransportPositionType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"deliveryStatus","deliveryStatus":"inTransport","inTransportETA":{"from":"now","to":1735741800}})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "inTransportEtaType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","paymentMethods":[{"method":"cash"}]})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "missingTotalAmount"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","totalAmount":12.5,"paymentMethods":[{"method":"cash"}]})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "totalAmountType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","totalAmount":1200,"paymentMethods":{"method":"cash"}})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "paymentMethodsType"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","totalAmount":1200,"paymentMethods":[{}]})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "paymentMethodMissingMethod"},
        PayloadTextParams{
            .messageType = Communication::MessageType::Reply,
            .payload =
                R"_({"transactionID":"ABCD-EFGH","version":3,"communicationType":"paymentInfo","totalAmount":1200,"paymentMethods":[{"method":"cashier"}]})_",
            .expectedOutcome = failure,
            .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
            .testName = "paymentMethodInvalid"},
        PayloadTextParams{.messageType = Communication::MessageType::Reply,
                          .payload = kReplyPaymentMethodUrlTooLong,
                          .expectedOutcome = failure,
                          .expectedMessage = "Invalid payload: does not conform to expected JSON schema",
                          .testName = "paymentMethodUrlTooLong"}),
    &CommunicationPayloadV3TestP::name);
