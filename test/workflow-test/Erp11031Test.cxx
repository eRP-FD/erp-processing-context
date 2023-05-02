/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "erp/ErpRequirements.hxx"

#include <gtest/gtest.h>

#include <zlib.h>

#include <chrono>
#include <cstddef>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <string_view>

using namespace std::chrono_literals;

namespace
{
    constexpr std::size_t compressBufferSize = 2048;
    constexpr std::string_view pnwTimestampFormat = "%Y%m%d%H%M%S";
    constexpr std::size_t pnwFormattedTimestampLength = 14;
    constexpr std::size_t validPnwResultValue = 2;
    constexpr std::string_view pnwPzNumber = "ODAyNzY4ODEwMjU1NDg0MzEzMDEwMDAwMDAwMDA2Mzg2ODc4MjAyMjA4MzEwODA3MzY=";

    /**
     * This function takes the base64-gzipped PNW (XML-representation of PNW, gzipped to binary and base64-encoded)
     * and performs URL-encoding onto it.
     */
    std::string urlEncodePnw(const std::string& data)
    {
        std::stringstream stream{};
        stream << std::hex;

        for (const auto character : data)
        {
            if ('+' == character || '/' == character || '=' == character)
            {
                stream << '%' << std::setw(2) << static_cast<int>(character);
            }
            else
            {
                stream << character;
            }
        }

        return stream.str();
    }

    /**
     * This function takes the XML-representation of the PNW and deflates it (gzip format) returning binary
     */
    std::string gzipPnw(const std::string& data)
    {
        // Initialize deflation. Let zlib choose alloc and free functions.
        z_stream stream{};
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        // Initialize zlib compression for gzip.
        Expect(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) == Z_OK,
               "could not initialize compression for gzip");

        std::uint8_t buffer[compressBufferSize];
        stream.avail_in = gsl::narrow<uInt>(data.size());
        stream.next_in = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data()));

        // Use Z_FINISH as 'flush' argument for `deflate()` because the whole input is available
        // to `deflate()` and the only reason why we might need to loop more than once, is that the
        // output buffer is not large enough.  In most cases, though, for metadata strings, the
        // buffer should be large enough and one iteration should be enough.
        constexpr auto flush = Z_FINISH;

        std::string dataResult{};
        do
        {
            stream.avail_out = sizeof(buffer);
            stream.next_out = buffer;

            // Deflate the next part of the input. This will either use up all remaining input or fill
            // up the output buffer.
            const int result = deflate(&stream, flush);
            Expect(result >= Z_OK, "compression failed");

            // Append the compressed data to the result string.
            const size_t pendingSize = sizeof(buffer) - stream.avail_out ;
            dataResult += std::string_view(reinterpret_cast<char*>(buffer), pendingSize);

            // Exit the loop when all input and output has been processed. This avoids one last
            // call to deflate().
            if (result == Z_STREAM_END)
            {
                break;
            }
        }
        while(stream.avail_out == 0);

        // Release any dynamic memory.
        const int result = deflateEnd(&stream);
        Expect(result == Z_OK, "finishing compression failed");

        return dataResult;
    }

    std::string convertTimestampToPnwFormat(const model::Timestamp& timestamp)
    {
        const auto timestampTimeT = timestamp.toTimeT();
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        const auto* timestampTm = std::gmtime(&timestampTimeT);

        std::string result(pnwFormattedTimestampLength + 1, 0);
        Expect(pnwFormattedTimestampLength == strftime(result.data(),
                                                       result.length(),
                                                       pnwTimestampFormat.data(),
                                                       timestampTm),
               "Failed to serialize timestamp");

        result.resize(pnwFormattedTimestampLength);
        return result;
    }

    struct PnwPzNumberInput
    {
        PnwPzNumberInput(std::string data, bool multipleNodes = false)
        : data{std::move(data)},
          multipleNodes{multipleNodes}
        {

        }

        std::string data;
        bool multipleNodes;
    };

    std::string createFullyEncodedPnw(
        std::optional<std::string> formattedTimestamp = std::nullopt,
        std::optional<std::string> resultValue = std::nullopt,
        std::optional<PnwPzNumberInput> pnwPzNumberInput = std::nullopt)
    {
        if (!formattedTimestamp.has_value())
        {
            formattedTimestamp = convertTimestampToPnwFormat(model::Timestamp::now());
        }

        if (!resultValue.has_value())
        {
            resultValue = std::to_string(validPnwResultValue);
        }

        std::string pzNumberNode{};
        if (pnwPzNumberInput.has_value())
        {
            pzNumberNode = "<PZ>" + pnwPzNumberInput->data + "</PZ>";
            if (pnwPzNumberInput->multipleNodes)
            {
                pzNumberNode += pzNumberNode;
            }
        }

        const std::string decodedPnw{"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><PN xmlns=\"http://ws.gematik.de/fa/vsdm/pnw/v1.0\" CDM_VERSION=\"1.0.0\"><TS>" +
                                     *formattedTimestamp +
                                     "</TS><E>" +
                                     *resultValue +
                                     "</E>" +
                                     pzNumberNode +
                                      "</PN>"};

        const auto gzippedPnw = gzipPnw(decodedPnw);
        const auto base64Pnw = Base64::encode(gzippedPnw);

        return urlEncodePnw(base64Pnw);
    }
}

class Erp11031Test : public ErpWorkflowTest
{
public:
    void createActivatedTask(
        std::optional<model::PrescriptionId>& prescriptionId,
        std::string& kvnr)
    {
        std::string accessCode{};
        ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

        ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));

        std::string qesBundle{};
        std::vector<model::Communication> communications{};
        ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));
    }

    void SetUp() override
    {
        GTEST_SKIP();
    }
};

TEST_F(Erp11031Test, BadKvnr)
{
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    createActivatedTask(prescriptionId, kvnr);

    ASSERT_FALSE(kvnr.empty());
    kvnr.pop_back();

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                            std::string{},
                                            HttpStatus::BadRequest,
                                            model::OperationOutcome::Issue::Type::invalid,
                                            "Missing or invalid KVNR query parameter",
                                            createFullyEncodedPnw()));

    ASSERT_FALSE(tasks);
}

TEST_F(Erp11031Test, BadPnwMissing)
{
    A_22432.test("Missing PNW causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    createActivatedTask(prescriptionId, kvnr);

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                            std::string{},
                                            HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Missing or invalid PNW query parameter",
                                            std::string{}));

    ASSERT_FALSE(tasks);
}

TEST_F(Erp11031Test, BadPnwCannotDecode)
{
    A_22432.test("Undecodable PNW causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    createActivatedTask(prescriptionId, kvnr);

    auto pnw = createFullyEncodedPnw();
    ASSERT_GT(pnw.length(), 6);
    pnw[3] = '0';
    pnw[4] = '0';
    pnw[5] = '0';

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                            std::string{},
                                            HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Failed decoding PNW data.",
                                            pnw));

    ASSERT_FALSE(tasks);
}

TEST_F(Erp11031Test, TimestampTooOld)
{
    A_22432.test("PNW with timestamp older than 30 minutes causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    createActivatedTask(prescriptionId, kvnr);

    std::optional<model::Bundle> tasks{};
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                            std::string{},
                                            HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "PNW is older than 30 minutes",
                                            createFullyEncodedPnw(
                                                convertTimestampToPnwFormat(model::Timestamp::now() - 31min))));

    ASSERT_FALSE(tasks);
}

TEST_F(Erp11031Test, TimestampInvalid)
{
    A_22432.test("PNW with invalid timestamp causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    createActivatedTask(prescriptionId, kvnr);

    std::optional<model::Bundle> tasks{};
    std::string invalidTsTrailing = convertTimestampToPnwFormat(model::Timestamp::now()) + "_";
    for(const auto& str : { "", "_20221006101559", "20229906101559", invalidTsTrailing.c_str(), "invalid" })
    {
        ASSERT_NO_FATAL_FAILURE(
            tasks = taskGet(kvnr, std::string{}, HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden,
                            "Failed parsing TS in PNW.", createFullyEncodedPnw(str), std::string("3-SMC-B-Testkarte-") + str));
        ASSERT_FALSE(tasks);
    }

}

TEST_F(Erp11031Test, ResultValueUnsupported)
{
    A_22432.test("PNW with unsupported (i.e. not configured) result value causes error message with code 403");
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    std::optional<model::Bundle> tasks{};

    createActivatedTask(prescriptionId, kvnr);

    for(const auto& value : { "0", "6", "100", "invalid", "5invalid"})
    {
        ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr, std::string{}, HttpStatus::Forbidden,
                                                model::OperationOutcome::Issue::Type::forbidden,
                                                "Ergebnis im Prüfungsnachweis ist nicht gültig.",
                                                createFullyEncodedPnw(std::nullopt, value),
                                                std::string("3-SMC-B-Testkarte-") + value));
        ASSERT_FALSE(tasks);
    }
}

TEST_F(Erp11031Test, BadPnwIndistinctPzNumber)
{
    A_22432.test("PNW with indistinct PZ causes error message with code 403");
    const auto startTime = model::Timestamp::now();

    std::string accessCode{};
    std::optional<model::PrescriptionId> prescriptionId{};
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));

    std::string kvnr{};
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));

    std::string qesBundle{};
    std::vector<model::Communication> communications{};
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    std::optional<model::Bundle> tasks{};
    auto pnwPzNumberInput =
        std::make_optional<PnwPzNumberInput>(pnwPzNumber.data(), true); // creates two "PZ" elements;
    ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                            std::string{},
                                            HttpStatus::Forbidden,
                                            model::OperationOutcome::Issue::Type::forbidden,
                                            "Failed parsing PNW XML.",
                                            createFullyEncodedPnw(
                                                std::nullopt,
                                                std::nullopt,
                                                std::move(pnwPzNumberInput))));

    ASSERT_FALSE(tasks);

    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();

    ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
        { prescriptionId, prescriptionId, std::nullopt },
        kvnr,
        "en",
        startTime,
        { telematikIdDoctor, kvnr, telematikIdPharmacy },
        { 0, 2 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read }));
}

TEST_F(Erp11031Test, ValidWithPnwPzNumber)
{
    A_22432.test("Valid PNW with PZ");
    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    std::optional<model::Bundle> tasks{};

    const auto pnwPzNumberInput = std::make_optional<PnwPzNumberInput>(pnwPzNumber.data());

    for(const auto& value : { "1", "2", "3", "5"})
    {
        const auto startTime = model::Timestamp::now();
        createActivatedTask(prescriptionId, kvnr);

        const auto telematikIdPharmacy = std::string("3-SMC-B-Testkarte-") + value;
        ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                                std::string{},
                                                HttpStatus::OK,
                                                std::nullopt,
                                                std::nullopt,
                                                createFullyEncodedPnw(std::nullopt, value, pnwPzNumberInput),
                                                telematikIdPharmacy));

        ASSERT_TRUE(tasks);
        ASSERT_EQ(tasks->getResourceCount(), 1);

        ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
            { prescriptionId, prescriptionId, std::nullopt },
            kvnr,
            "en",
            startTime,
            { telematikIdDoctor, kvnr, telematikIdPharmacy },
            { 0, 2 },
            { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read }));
    }
}

TEST_F(Erp11031Test, ValidWithoutPnwPzNumber)
{
    A_22432.test("Valid PNW without PZ");
    const auto telematikIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::PrescriptionId> prescriptionId{};
    std::string kvnr{};
    std::optional<model::Bundle> tasks{};

    for(const auto& value : { "1", "2", "3", "5"})
    {
        const auto startTime = model::Timestamp::now();
        createActivatedTask(prescriptionId, kvnr);

        const auto telematikIdPharmacy = std::string("3-SMC-B-Testkarte-") + value;
        ASSERT_NO_FATAL_FAILURE(tasks = taskGet(kvnr,
                                                std::string{},
                                                HttpStatus::OK,
                                                std::nullopt,
                                                std::nullopt,
                                                createFullyEncodedPnw(std::nullopt, value),
                                                telematikIdPharmacy));

        ASSERT_TRUE(tasks);
        ASSERT_EQ(tasks->getResourceCount(), 1);

        ASSERT_NO_FATAL_FAILURE(checkAuditEvents(
            { prescriptionId, prescriptionId, std::nullopt },
            kvnr,
            "en",
            startTime,
            { telematikIdDoctor, kvnr, telematikIdPharmacy },
            { 0, 2 },
            { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read }));
    }
}
