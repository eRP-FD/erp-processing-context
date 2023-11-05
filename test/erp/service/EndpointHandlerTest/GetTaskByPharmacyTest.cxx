/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/compression/Deflate.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/hsm/VsdmKeyBlobDatabase.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Random.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTest.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>

namespace
{

std::string makePz(const model::Kvnr& kvnr, const model::Timestamp& timestamp, char updateReason,
                   const VsdmHmacKey& keyPackage)
{
    std::string output;
    // the output is base64 encoded data of:
    // * 10 bytes KVNR
    // * 10 bytes timestamp
    // * 1 byte operator Id of hmac key
    // * 1 byte version of hmac key
    // * 24 bytes truncated hmac sha256 of the data above
    output.resize(23 + 24);
    std::string kvnrStr = kvnr.id();

    auto pzIt = std::copy(kvnrStr.begin(), kvnrStr.end(), output.begin());
    const auto secondsSinceEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(timestamp.toChronoTimePoint().time_since_epoch());
    const auto epochStr = std::to_string(secondsSinceEpoch.count());
    pzIt = std::copy(epochStr.begin(), epochStr.end(), pzIt);
    *pzIt = updateReason;
    pzIt++;
    *pzIt = keyPackage.operatorId();
    pzIt++;
    *pzIt = keyPackage.version();
    pzIt++;
    std::string_view validationDataForHash = std::string_view{output.begin(), pzIt};
    auto hmacKey = Base64::decode(keyPackage.plainTextKey());

    const auto calculatedHash = util::bufferToString(Hash::hmacSha256(hmacKey, validationDataForHash));
    std::copy(calculatedHash.begin(), calculatedHash.begin() + 24, pzIt);
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

    return Base64::encode(gzippedPnw);
}

}// namespace


class GetTasksByPharmacyTest : public EndpointHandlerTest
{
public:
    void SetUp() override
    {
        EndpointHandlerTest::SetUp();

        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);

        keyPackage.setPlainTextKey(Base64::encode(key));

        auto hsmPoolSession = mServiceContext.getHsmPool().acquire();
        auto& hsmSession = hsmPoolSession.session();
        const ErpVector vsdmKeyData = ErpVector::create(keyPackage.serializeToString());
        ErpBlob vsdmKeyBlob = hsmSession.wrapRawPayload(vsdmKeyData, 0);

        auto& vsdmBlobDb = mServiceContext.getVsdmKeyBlobDatabase();
        VsdmKeyBlobDatabase::Entry blobEntry;
        blobEntry.operatorId = keyPackage.operatorId();
        blobEntry.version = keyPackage.version();
        blobEntry.createdDateTime = model::Timestamp::now().toChronoTimePoint();
        blobEntry.blob = vsdmKeyBlob;
        vsdmBlobDb.storeBlob(std::move(blobEntry));
    }

    void callHandler(const std::string& pnw, ServerResponse& serverResponse)
    {
        GetAllTasksHandler handler({});

        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
        Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setAccessToken(jwtPharmacy);
        // store the unescaped string, the real handler also gets the the unescaped string
        serverRequest.setQueryParameters({{"pnw", pnw}});
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        handler.handleRequest(sessionContext);
    }

    void TearDown() override
    {
        auto& vsdmBlobDb = mServiceContext.getVsdmKeyBlobDatabase();
        vsdmBlobDb.deleteBlob(keyPackage.operatorId(), keyPackage.version());
        EndpointHandlerTest::TearDown();
    }

protected:
    VsdmHmacKey keyPackage{'!', '1'};
    model::Kvnr kvnr{"X123456788"};
};

TEST_F(GetTasksByPharmacyTest, success)
{
    auto pz = makePz(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse));

    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    model::Bundle taskBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
    const auto tasks = taskBundle.getResourcesByType<model::Task>("Task");
    A_23452.test("task.status=ready and task.for=kvnr");
    // see MockDatabase::fillWithStaticData() for the above KVNR with status ready
    EXPECT_EQ(tasks.size(), 3);
    for (const auto& task : tasks)
    {
        ASSERT_NO_THROW((void) model::Task::fromXml(task.serializeToXmlString(), *StaticData::getXmlValidator(),
                                                    *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
        ASSERT_EQ(task.status(), model::Task::Status::ready);
        EXPECT_EQ(task.kvnr(), kvnr);
    }
}


TEST_F(GetTasksByPharmacyTest, missingKey)
{
    A_23455.test("missing value for 'pz' in xml");
    auto pnw = createEncodedPnw();
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Prüfziffer "
                                      "fehlt im VSDM Prüfungsnachweis).");
}


TEST_F(GetTasksByPharmacyTest, outdatedData)
{
    using namespace std::chrono_literals;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "0");
    auto pz = makePz(kvnr, model::Timestamp::now() - 1min, 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}

TEST_F(GetTasksByPharmacyTest, dateTooNew)
{
    A_23451.test("PNW with timestamp too new minutes causes error message with code 403");
    using namespace std::chrono_literals;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "60");
    auto pz = makePz(kvnr, model::Timestamp::now() + 2min, 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}


// GEMREQ-start A_23456#test
TEST_F(GetTasksByPharmacyTest, unknownKey)
{
    VsdmHmacKey newKeyPackage{'$', '1'};
    newKeyPackage.setPlainTextKey(keyPackage.plainTextKey());
    auto pz = makePz(kvnr, model::Timestamp::now(), 'U', newKeyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                      "Prüfung der HMAC-Sicherung).");
}


TEST_F(GetTasksByPharmacyTest, differentKey)
{
    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));
    auto pz = makePz(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                      "Prüfung der HMAC-Sicherung).");
}
// GEMREQ-end A_23456#test

TEST_F(GetTasksByPharmacyTest, pzTooShort)
{
    auto pz = makePz(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pzBin = Base64::decode(pz);
    pzBin.resize(46);
    auto pnw = createEncodedPnw(Base64::encode(pz));
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                          "Failed parsing PNW XML.", "Invalid size of Prüfziffer");
}

TEST_F(GetTasksByPharmacyTest, pzTooLong)
{
    auto pz = makePz(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pzBin = Base64::decode(pz);
    pzBin.push_back('A');
    auto pnw = createEncodedPnw(Base64::encode(pz));
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callHandler(pnw, serverResponse), HttpStatus::Forbidden,
                                          "Failed parsing PNW XML.", "Invalid size of Prüfziffer");
}

TEST_F(GetTasksByPharmacyTest, invalidXml)
{
    std::string_view pnwData = "abc12345";
    const auto gzippedPnw = Deflate().compress(pnwData, Compression::DictionaryUse::Undefined);
    auto pnw = Base64::encode(gzippedPnw);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
        callHandler(pnw, serverResponse), HttpStatus::Forbidden, "Failed parsing PNW XML.",
        "xml could not be parsed, error 4, at line 1, message: Start tag expected, '<' not found");
}
