/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/VsdmProof.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/compression/Deflate.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Hash.hxx"
#include "src/shared/enrolment/VsdmHmacKey.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;

namespace
{

std::string makePzV1(const model::Kvnr& kvnr, const model::Timestamp& timestamp,
                   const std::optional<VsdmHmacKey>& keyPackage = std::nullopt)
{
    std::string output;
    output.resize(23 + 24);
    std::string kvnrStr = kvnr.id();

    auto pzIt = std::copy(kvnrStr.begin(), kvnrStr.end(), output.begin());
    const auto secondsSinceEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(timestamp.toChronoTimePoint().time_since_epoch());
    const auto epochStr = std::to_string(secondsSinceEpoch.count());
    pzIt = std::copy(epochStr.begin(), epochStr.end(), pzIt);
    *pzIt = 'U';
    pzIt++;
    if (keyPackage)
    {
        *pzIt = keyPackage->operatorId();
        pzIt++;
        *pzIt = keyPackage->version();
        pzIt++;
        std::string_view validationDataForHash = std::string_view{output.begin(), pzIt};
        auto hmacKey = Base64::decode(keyPackage->plainTextKey());
        const auto calculatedHash = util::bufferToString(Hash::hmacSha256(hmacKey, validationDataForHash));
        std::copy(calculatedHash.begin(), calculatedHash.begin() + 24, pzIt);
    }
    else
    {
        *pzIt = 'A';
        pzIt++;
        *pzIt = '1';
        pzIt++;
        std::fill(pzIt, output.end(), 'F');
    }
    return Base64::encode(output);
}


std::string createEncodedPnw(std::optional<std::string> pnwPzNumberInput = std::nullopt)
{
    std::string pnwXml{
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><PN xmlns="http://ws.gematik.de/fa/vsdm/pnw/v1.0" CDM_VERSION="1.0.0"><TS>20230303111110</TS>)"};
    if (pnwPzNumberInput.has_value())
    {
        pnwXml += "<E>1</E><PZ>" + pnwPzNumberInput.value() + "</PZ>";
    }
    else
    {
        pnwXml += "<E>3</E><EC>12101</EC>";
    }
    pnwXml += "</PN>";

    const auto gzippedPnw = Deflate().compress(pnwXml, Compression::DictionaryUse::Undefined);
    const auto base64Pnw = Base64::encode(gzippedPnw);
    return UrlHelper::escapeUrl(base64Pnw);
}

std::string createEncodedPnwWithError(std::optional<std::string> pnwResult = std::nullopt)
{
    std::string pnwXml{
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><PN xmlns="http://ws.gematik.de/fa/vsdm/pnw/v1.0" CDM_VERSION="1.0.0"><TS>20230303111110</TS>)"};
    if (pnwResult.has_value())
    {
        pnwXml += "<E>" + pnwResult.value() + "</E>";
    }
    else
    {
        pnwXml += "<E>6</E>";
    }
    pnwXml += "</PN>";

    const auto gzippedPnw = Deflate().compress(pnwXml, Compression::DictionaryUse::Undefined);
    const auto base64Pnw = Base64::encode(gzippedPnw);
    return UrlHelper::escapeUrl(base64Pnw);
}

}// namespace

class GetTaskByPharmacyWfTest : public ErpWorkflowTest
{
public:
    void SetUp() override
    {
        ASSERT_NO_FATAL_FAILURE(kvnr = generateNewRandomKVNR());
    }

    void createActivatedTask(std::optional<model::PrescriptionId>& prescriptionId)
    {
        std::string accessCode{};
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
        std::string qesBundle{};
        std::vector<model::Communication> communications{};
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr.id(), accessCode));
    }

protected:
    model::Kvnr kvnr{""};
    std::string hcv = VsdmProof2::makeHcv("20250101", "Hauptstr");
};


TEST_F(GetTaskByPharmacyWfTest, BadPnwMissing)
{
    A_25206.test("PNW without PZ and results other then 3 causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Missing or invalid PNW query parameter", std::string{}));

    ASSERT_FALSE(tasks);
}

TEST_F(GetTaskByPharmacyWfTest, BadPnwCannotDecode)
{
    A_23450.test("Undecodable PNW causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);

    auto pnw = createEncodedPnw();
    ASSERT_GT(pnw.length(), 6);
    pnw[3] = '0';
    pnw[4] = '0';
    pnw[5] = '0';

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Failed decoding PNW data.", pnw));

    ASSERT_FALSE(tasks);
}

TEST_F(GetTaskByPharmacyWfTest, TimestampTooOld)
{
    A_23451_01.test("PNW with timestamp older than 30 minutes causes error message with code 403");
    const auto startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);

    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    std::optional<VsdmHmacKey> vsdmKey{keys[0]};

    const auto pz = makePzV1(kvnr, model::Timestamp::now() - 31min, vsdmKey);
    const auto pnw = createEncodedPnw(pz);

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden "
                                            "(Zeitliche Gültigkeit des Anwesenheitsnachweis überschritten).",
                                            pnw));
    ASSERT_FALSE(tasks);

    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, KvnrInvalid)
{
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    std::optional<VsdmHmacKey> vsdmKey{keys[0]};
    auto pzBin = Base64::decodeToString(makePzV1(kvnr, model::Timestamp::now(), vsdmKey));
    const auto pnw = createEncodedPnw(Base64::encode(pzBin));

    const model::Kvnr kvnrInvalid{"AA235678"};

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnrInvalid.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden, "Invalid Kvnr", pnw));
    ASSERT_FALSE(tasks);
}

TEST_F(GetTaskByPharmacyWfTest, RejectWithoutPnwPzNumber)
{
    A_23450.test("Forbidden PNW without PZ");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::optional<model::Bundle> tasks{};
    createActivatedTask(prescriptionId);
    const auto pnw = createEncodedPnwWithError("5");

    ASSERT_NO_FATAL_FAILURE(tasks =
                                taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                        model::OperationOutcome::Issue::Type::forbidden,
                                        "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Prüfziffer "
                                        "fehlt im VSDM Prüfungsnachweis oder ungültiges Ergebnis im Prüfungsnachweis).",
                                        pnw));
    ASSERT_FALSE(tasks);
}


TEST_F(GetTaskByPharmacyWfTest, Success)
{
    A_23450.test("Successful GET Task by pharmacy with PZ");
    const auto startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId{};
    std::optional<model::Bundle> tasks{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    std::optional<VsdmHmacKey> vsdmKey{keys[0]};

    const auto pz = makePzV1(kvnr, model::Timestamp::now(), vsdmKey);
    const auto pnw = createEncodedPnw(pz);

    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::OK, std::nullopt, std::nullopt, pnw));
    ASSERT_TRUE(tasks);
    ASSERT_EQ(tasks->getResourceCount(), 1);
    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, PN2_rejectMissingHcv)
{
    if (runsInCloudEnv())
    {
        GTEST_SKIP();
    }
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    auto encryptedProof = VsdmProof2::encrypt(vsdmKey, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    EnvironmentVariableGuard guardHcvCheck(ConfigurationKey::SERVICE_TASK_GET_ENFORCE_HCV_CHECK, "false");
    ServerResponse serverResponse;
    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::OK, std::nullopt, std::nullopt, pnw));
}

TEST_F(GetTaskByPharmacyWfTest, PN2Success)
{
    const auto startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId{};
    std::optional<model::Bundle> tasks{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    auto encryptedProof = VsdmProof2::encrypt(
        vsdmKey,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now(), .kvnr = kvnr});

    auto pz = encryptedProof.serialize();
    const auto pnw = createEncodedPnw(pz) + "&hcv=" + Base64::toBase64Url(Base64::encode(hcv));

    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::OK, std::nullopt, std::nullopt, pnw));
    ASSERT_TRUE(tasks);
    ASSERT_EQ(tasks->getResourceCount(), 1);
    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, PN2TimestampOutdated)
{
    const auto startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    auto encryptedProof = VsdmProof2::encrypt(
        vsdmKey,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now() - 31min, .kvnr = kvnr});

    auto pz = encryptedProof.serialize();
    const auto pnw = createEncodedPnw(pz) + "&hcv=" + Base64::toBase64Url(Base64::encode(hcv));

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden "
                                            "(Zeitliche Gültigkeit des Anwesenheitsnachweis überschritten).",
                                            pnw));
    ASSERT_FALSE(tasks);

    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, PN2HcvMismatch)
{
    const auto startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    auto encryptedProof = VsdmProof2::encrypt(
        vsdmKey,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now(), .kvnr = kvnr});

    auto pz = encryptedProof.serialize();
    const auto pnw = createEncodedPnw(pz) + "&hcv=" +  Base64::toBase64Url(Base64::encode(VsdmProof2::makeHcv("20240101", "Nebenstr.")));

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::RejectHcvMismatch,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "HCV Validierung fehlgeschlagen.",
                                            pnw));
    ASSERT_FALSE(tasks);

    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, PN2KvnrMismatch)
{
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    const model::Kvnr kvnr2{"X123456788"};
    auto encryptedProof = VsdmProof2::encrypt(
        vsdmKey,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now(), .kvnr = kvnr2});

    auto pz = encryptedProof.serialize();
    const auto pnw = createEncodedPnw(pz) + "&hcv=" + Base64::encode(Base64::toBase64Url(hcv));

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::KvnrMismatch,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "KVNR Überprüfung fehlgeschlagen.",
                                            pnw));
    ASSERT_FALSE(tasks);
}


TEST_F(GetTaskByPharmacyWfTest, SuccessAcceptPN3)
{
    GTEST_SKIP_("[ERP-19096] cannot run, because the two test clients start two independent servers and while Jenkins the port is noch reachable.");
    {
        // prepare: enabel accept PN3
        const auto expiry = model::Timestamp::now() + std::chrono::hours(12);
        std::unique_ptr<TestClient> client = TestClient::create(StaticData::getXmlValidator(), TestClient::Target::ADMIN);
        ClientRequest clientRequest(
            Header(
                HttpMethod::PUT,
                "/admin/configuration",
                Header::Version_1_1,
                {{Header::Host, client->getHostHttpHeader()},
                {Header::Authorization, "Basic cred"},
                {Header::ContentType, ContentMimeType::xWwwFormUrlEncoded}},
                HttpStatus::Unknown
            ),
                "AcceptPN3=true&AcceptPN3Expiry=" + expiry.toXsDateTime()
            );
        auto response = client->send(clientRequest);
        EXPECT_EQ(response.getHeader().status(), HttpStatus::OK);
    }

    A_25209.test("Successful GET Task by pharmacy with PN3");
    const auto startTime = model::Timestamp::now();
    std::optional<model::PrescriptionId> prescriptionId{};
    std::optional<model::Bundle> tasks{};
    createActivatedTask(prescriptionId);

    const auto pnw = createEncodedPnw();

    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), "kvnr="+kvnr.id(), HttpStatus::Accepted, std::nullopt, std::nullopt, pnw));
    ASSERT_TRUE(tasks);
    ASSERT_EQ(tasks->getResourceCount(), 1);
    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        {prescriptionId, prescriptionId, std::nullopt}, kvnr.id(), "en", startTime,
        {telematikIdDoctor, kvnr.id(), telematikIdPharmacy}, {0, 2},
        {model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read}));
}

TEST_F(GetTaskByPharmacyWfTest, recoverSecret)
{
    std::string accessCode{};
    std::optional<model::PrescriptionId> prescriptionId{};
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    std::string qesBundle{};
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr.id(), *prescriptionId, model::Timestamp::now())));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = std::get<model::Task>(taskActivate(*prescriptionId, accessCode, qesBundle)));

    ASSERT_NO_FATAL_FAILURE(taskAccept(*prescriptionId, accessCode));

    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::Bundle> bundle;
    ASSERT_NO_FATAL_FAILURE(bundle = taskGetId(*prescriptionId, telematikIdPharmacy, accessCode, std::nullopt,
                                               HttpStatus::OK, std::nullopt, false, "GET /Task/<id>?ac"));
}

TEST_F(GetTaskByPharmacyWfTest, RateLimit_PN2HcvMismatch)
{
    if (!client->getContext())
    {
        // Skip as integration test
        GTEST_SKIP();
    }
    EnvironmentVariableGuard limit{ConfigurationKey::SERVICE_TASK_GET_RATE_LIMIT, "3"};

    client->getContext()->getRedisClient()->flushdb();

    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    const auto& keys = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_VSDM_KEYS);
    ASSERT_FALSE(keys.empty());
    VsdmHmacKey vsdmKey{keys[0]};

    auto encryptedProof = VsdmProof2::encrypt(
        vsdmKey,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now(), .kvnr = kvnr});

    auto pz = encryptedProof.serialize();
    const auto pnw = createEncodedPnw(pz) + "&hcv=" +  Base64::toBase64Url(Base64::encode(VsdmProof2::makeHcv("20240101", "Nebenstr.")));

    for (int i = 0; i < 3; ++i)
    {
        std::optional<model::Bundle> tasks{};
        ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::RejectHcvMismatch,
                                                model::OperationOutcome::Issue::Type::forbidden,
                                                "HCV Validierung fehlgeschlagen.",
                                                pnw));
        ASSERT_FALSE(tasks);
    }

    {
        std::optional<model::Bundle> tasks{};
        ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{},
                                                HttpStatus::Locked,
                                                model::OperationOutcome::Issue::Type::forbidden, "TelematikId blocked for today.", pnw));
    }
}
