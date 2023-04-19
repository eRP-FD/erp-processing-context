/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#include "erp/ErpRequirements.hxx"
#include "erp/compression/Deflate.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/util/Hash.hxx"
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

std::string makePz(const model::Kvnr& kvnr, const model::Timestamp& timestamp,
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
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><PN xmlns="http://ws.gematik.de/fa/vsdm/pnw/v1.0" CDM_VERSION="1.0.0"><TS>20230303111110</TS><E>1</E>)"};
    if (pnwPzNumberInput.has_value())
    {
        pnwXml += "<PZ>" + pnwPzNumberInput.value() + "</PZ>";
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
};


TEST_F(GetTaskByPharmacyWfTest, BadPnwMissing)
{
    A_23455.test("Missing PNW causes error message with code 403");
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
    A_23451.test("PNW with timestamp older than 30 minutes causes error message with code 403");
    const auto startTime = model::Timestamp::now();

    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);

    const auto pz = makePz(kvnr, model::Timestamp::now() - 31min);
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


TEST_F(GetTaskByPharmacyWfTest, TimestampInvalid)
{
    A_23450.test("PNW with invalid timestamp causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    auto pzBin = Base64::decodeToString(makePz(kvnr, model::Timestamp::now()));
    // first 10 chars are kvnr, followed by timestamp
    pzBin[10] = '-';
    const auto pnw = createEncodedPnw(Base64::encode(pzBin));

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden "
                                            "(Zeitliche Gültigkeit des Anwesenheitsnachweis überschritten).",
                                            pnw));
    ASSERT_FALSE(tasks);
}

TEST_F(GetTaskByPharmacyWfTest, KvnrInvalid)
{
    std::optional<model::PrescriptionId> prescriptionId{};
    createActivatedTask(prescriptionId);
    auto pzBin = Base64::decodeToString(makePz(kvnr, model::Timestamp::now()));
    // first 10 chars are kvnr
    pzBin[2] = 'A';
    const auto pnw = createEncodedPnw(Base64::encode(pzBin));

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden, "Invalid Kvnr", pnw));
    ASSERT_FALSE(tasks);
}

TEST_F(GetTaskByPharmacyWfTest, RejectWithoutPnwPzNumber)
{
    A_23450.test("Forbidden PNW without PZ");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::optional<model::Bundle> tasks{};
    createActivatedTask(prescriptionId);
    const auto pnw = createEncodedPnw();

    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr.id(), std::string{}, HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden "
                                            "(Prüfziffer fehlt im VSDM Prüfungsnachweis).",
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

    const auto pz = makePz(kvnr, model::Timestamp::now(), vsdmKey);
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
