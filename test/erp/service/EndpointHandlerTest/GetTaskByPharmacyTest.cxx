/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/VsdmProof.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/compression/Deflate.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/hsm/VsdmKeyBlobDatabase.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/Random.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <date/tz.h>
#include <erp/service/task/CloseTaskHandler.hxx>
#include <gtest/gtest.h>
#include <chrono>
using namespace std::chrono_literals;

namespace
{

std::string makePzV1(const model::Kvnr& kvnr, const model::Timestamp& timestamp, char updateReason,
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

    return Base64::encode(gzippedPnw);
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

    void callHandler(const std::string& pnw, ServerResponse& serverResponse,
                     std::optional<model::Kvnr> kvnr = std::nullopt, std::optional<std::string> hcv = std::nullopt,
                     std::optional<int> count = std::nullopt, std::optional<std::string> encodedHcv = std::nullopt)
    {
        GetAllTasksHandler handler({});

        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
        Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setAccessToken(jwtPharmacy);
        ServerRequest::QueryParametersType params{{"pnw", pnw}};
        if (kvnr.has_value())
        {
            params.emplace_back("kvnr", kvnr.value().id());
        }
        if (count.has_value())
        {
            params.emplace_back("_count", std::to_string(count.value()));
        }
        if (hcv.has_value())
        {
            params.emplace_back("hcv", Base64::toBase64Url(Base64::encode(hcv.value())));
        }
        if (encodedHcv.has_value())
        {
            params.emplace_back("hcv", encodedHcv.value());
        }
        serverRequest.setQueryParameters(params);
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        handler.handleRequest(sessionContext);
    }

    void verifyTasksReady(ServerResponse serverResponse, size_t expectTasksSize = 2,
                          const std::string& expectedHcvUrlParam = "")
    {
        model::Bundle taskBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
        const auto tasks = taskBundle.getResourcesByType<model::Task>("Task");
        A_23452_02.test("task.status=ready and task.for=kvnr");
        // see MockDatabase::fillWithStaticData() for the above KVNR with status ready
        EXPECT_EQ(tasks.size(), expectTasksSize);
        auto acceptedExpiryDate = model::Timestamp::fromGermanDate(model::Timestamp::now().toGermanDate());
        for (const auto& task : tasks)
        {
            ASSERT_NO_THROW((void) model::Task::fromXml(task.serializeToXmlString(), *StaticData::getXmlValidator()));
            ASSERT_EQ(task.status(), model::Task::Status::ready);
            EXPECT_EQ(task.kvnr(), kvnr);
            TLOG(INFO) << "GET task with expiryDate = " << task.expiryDate().toGermanDate();
            ASSERT_GE(task.expiryDate(), acceptedExpiryDate);
            EXPECT_EQ(task.type(), model::PrescriptionType::apothekenpflichigeArzneimittel);
            EXPECT_NO_THROW(auto accessCode [[maybe_unused]] = task.accessCode());
        }
        if (!expectedHcvUrlParam.empty())
        {
            const auto link = taskBundle.getLink(model::Link::Self);
            ASSERT_TRUE(link.has_value());
            EXPECT_NE(link->find(expectedHcvUrlParam), std::string_view::npos) << *link;
        }
    }


    void setAcceptPN3(bool enable, const std::optional<model::Timestamp>& expiry)
    {
        auto runtimeConfig = mServiceContext.getRuntimeConfigurationSetter();
        if (enable)
        {
            runtimeConfig->enableAcceptPN3(expiry.has_value() ? expiry.value()
                                                              : model::Timestamp::now() + std::chrono::hours{1});
        }
        else
        {
            runtimeConfig->disableAcceptPN3();
        }
    }

    void TearDown() override
    {
        auto& vsdmBlobDb = mServiceContext.getVsdmKeyBlobDatabase();
        vsdmBlobDb.deleteBlob(keyPackage.operatorId(), keyPackage.version());
        EndpointHandlerTest::TearDown();
    }

    void validateLink(const model::Bundle &bundle, const model::Link::Type type, const std::string &expectPath)
    {
        const auto link = bundle.getLink(type);
        ASSERT_TRUE(link.has_value());
        const auto url = link.value();
        const auto pathStart = url.find("/Task");
        ASSERT_NE(pathStart, std::string::npos);
        const auto linkedPath = std::string(url.substr(pathStart));
        EXPECT_EQ(linkedPath, expectPath);
    }

    void insertTask(model::PrescriptionType prescriptionType,
            ResourceTemplates::TaskType taskType,
            const int64_t databaseId,
            const model::Timestamp& expirydate)
    {
        const auto taskId =
            model::PrescriptionId::fromDatabaseId(prescriptionType, databaseId);
        auto validUntil =
            date::make_zoned(model::Timestamp::GermanTimezone, expirydate.toChronoTimePoint() + 24h);
        validUntil = floor<date::days>(validUntil.get_local_time());
        ResourceTemplates::TaskOptions taskOpts{
            .taskType = taskType,
            .prescriptionId = taskId,
            .expirydate = model::Timestamp(validUntil.get_sys_time()),
            .kvnr = kvnr.id(),
        };
        auto doc = model::NumberAsStringParserDocument::fromJson(ResourceTemplates::taskJson(taskOpts));
        mockDatabase->insertTask(model::Task::fromJson(doc));
        TLOG(INFO) << "inserted task with expiry date " << taskOpts.expirydate;
    }

protected:
    VsdmHmacKey keyPackage{'A', '1'};
    model::Kvnr kvnr{"X123456788"};
    std::string hcv = VsdmProof2::makeHcv("20250101", "Hauptstr");
};


TEST_F(GetTasksByPharmacyTest, PN2_success)
{
    auto encryptedProof = VsdmProof2::encrypt(
        keyPackage,
        VsdmProof2::DecryptedProof{.revoked = false, .hcv = hcv, .iat = model::Timestamp::now(), .kvnr = kvnr});

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr, hcv));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    verifyTasksReady(serverResponse, 2, "hcv=" + Base64::toBase64Url(Base64::encode(hcv)));
}

TEST_F(GetTasksByPharmacyTest, PN2_rejectMissingHcv)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    EnvironmentVariableGuard guardHcvCheck(ConfigurationKey::SERVICE_TASK_GET_ENFORCE_HCV_CHECK, "true");
    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::RejectMissingHcv,
                                      "Ein Prüfnachweis ohne HCV wird nicht akzeptiert.");
}


TEST_F(GetTasksByPharmacyTest, PN2_rejectRevoked)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = true,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::Forbidden,
                                      "eGK gesperrt");
}


TEST_F(GetTasksByPharmacyTest, PN2_kvnrMismatch)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = model::Kvnr("Y123456788"),
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::KvnrMismatch,
                                      "KVNR Überprüfung fehlgeschlagen.");
}

TEST_F(GetTasksByPharmacyTest, PN2_hcvMismatch)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = VsdmProof2::makeHcv("20250101", "Nebenstr"),
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    // even when the force check is off, the validation has to be executed
    EnvironmentVariableGuard guardHcvCheck(ConfigurationKey::SERVICE_TASK_GET_ENFORCE_HCV_CHECK, "false");
    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::RejectHcvMismatch,
                                      "HCV Validierung fehlgeschlagen.");
}

TEST_F(GetTasksByPharmacyTest, PN2_hcvBase64Error)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now(),
                                                              .kvnr = kvnr,
                                                          });

    auto encodedHcv = Base64::toBase64Url(Base64::encode(hcv)) + "====";// invalid excessive padding

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    // even when the force check is off, the validation has to be executed
    EnvironmentVariableGuard guardHcvCheck(ConfigurationKey::SERVICE_TASK_GET_ENFORCE_HCV_CHECK, "false");
    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, std::nullopt, std::nullopt, encodedHcv),
                                      HttpStatus::BadRequest, "HCV Base64-Dekodierung fehlgeschlagen.");
}

TEST_F(GetTasksByPharmacyTest, PN2_missingKey)
{
    // use the same key, but with a different identifier
    VsdmHmacKey keyPackage2{gsl::narrow<char>(keyPackage.operatorId() + 1), keyPackage.version()};
    keyPackage2.setPlainTextKey(keyPackage.plainTextKey());
    auto encryptedProof = VsdmProof2::encrypt(keyPackage2, VsdmProof2::DecryptedProof{
                                                               .revoked = false,
                                                               .hcv = hcv,
                                                               .iat = model::Timestamp::now(),
                                                               .kvnr = kvnr,
                                                           });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (VSDM "
                                      "Schlüssel nicht vorhanden oder Inhalte nicht entschlüsselbar).");
}


TEST_F(GetTasksByPharmacyTest, PN2_outdatedTimestamp)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now() - 1min,
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "0");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}


TEST_F(GetTasksByPharmacyTest, PN2_timestampTooNew)
{
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{
                                                              .revoked = false,
                                                              .hcv = hcv,
                                                              .iat = model::Timestamp::now() + 2min,
                                                              .kvnr = kvnr,
                                                          });

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "60");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr, hcv), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}


TEST_F(GetTasksByPharmacyTest, success)
{
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr));

    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    verifyTasksReady(serverResponse);
}

TEST_F(GetTasksByPharmacyTest, successAcceptPN3)
{
    setAcceptPN3(true, std::nullopt);
    auto pnw = createEncodedPnw();
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr));

    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Accepted);
    verifyTasksReady(serverResponse);
}

TEST_F(GetTasksByPharmacyTest, invalidResult)
{
    A_25206.test("Result of PNW is not 3");
    auto pnw = createEncodedPnwWithError("5");
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Prüfziffer "
                                      "fehlt im VSDM Prüfungsnachweis oder ungültiges Ergebnis im Prüfungsnachweis).");
}

TEST_F(GetTasksByPharmacyTest, notAcceptPn3)
{
    A_25207.test("no 'pz' in xml and accept PN3 disabled");
    setAcceptPN3(false, std::nullopt);
    auto pnw = createEncodedPnw();
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::NotAcceptPN3,
                                      "Es wird kein Prüfnachweis mit Ergebnis 3 (ohne Prüfziffer) akzeptiert.");
}

TEST_F(GetTasksByPharmacyTest, RejectMissingKvnr)
{
    A_25208_01.test("For pharmacy, reject calls to endpoint GET /Task without query parameter `kvnr`.");
    setAcceptPN3(true, std::nullopt);
    auto pnw = createEncodedPnw();
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::RejectMissingKvnr,
                                      "Ein Prüfnachweis ohne KVNR wird nicht akzeptiert.");

    setAcceptPN3(false, std::nullopt);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse), HttpStatus::RejectMissingKvnr,
                                      "Ein Prüfnachweis ohne KVNR wird nicht akzeptiert.");
}

TEST_F(GetTasksByPharmacyTest, successAcceptPN3expired)
{
    using namespace std::chrono_literals;
    setAcceptPN3(true, model::Timestamp::now() - 1min);
    auto pnw = createEncodedPnw();
    ServerResponse serverResponse;

    ::testing::internal::CaptureStderr();
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr));

    std::string output = ::testing::internal::GetCapturedStderr();
    EXPECT_TRUE(output.find("AcceptPN3 expired") != std::string::npos);

    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Accepted);
    verifyTasksReady(serverResponse);
}

TEST_F(GetTasksByPharmacyTest, outdatedData)
{
    using namespace std::chrono_literals;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "0");
    auto pz = makePzV1(kvnr, model::Timestamp::now() - 1min, 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}

TEST_F(GetTasksByPharmacyTest, dateTooNew)
{
    A_23451_01.test("PNW with timestamp too new minutes causes error message with code 403");
    using namespace std::chrono_literals;
    EnvironmentVariableGuard guardTimestamp(ConfigurationKey::VSDM_PROOF_VALIDITY_SECONDS, "60");
    auto pz = makePzV1(kvnr, model::Timestamp::now() + 2min, 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Zeitliche "
                                      "Gültigkeit des Anwesenheitsnachweis überschritten).");
}

TEST_F(GetTasksByPharmacyTest, notExpired)
{
    // add good task to existing tasks
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::Ready,
        202830,
        model::Timestamp::now() +24h
    );
    // expired today but also good
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::Ready,
        202831,
        model::Timestamp::now()
    );
    // expired before today
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::Ready,
        202832,
        model::Timestamp::now() -48h
    );
    // expired before today and wrong task type
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::Draft,
        202833,
        model::Timestamp::now() -96h
    );
    // expired before today and wrong other task type
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::InProgress,
        202834,
        model::Timestamp::now() -96h
    );
    // not expiered but wrong task type
    insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
        ResourceTemplates::TaskType::InProgress,
        202836,
        model::Timestamp::now() +96h
    );
    // not expiered but wrong prescription type
    insertTask(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
        ResourceTemplates::TaskType::Ready,
        202837,
        model::Timestamp::now() +96h
    );

    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr));

    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    model::Bundle taskBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
    const auto tasks = taskBundle.getResourcesByType<model::Task>("Task");
    verifyTasksReady(serverResponse, 4);
}

// GEMREQ-start A_23456-01#test
TEST_F(GetTasksByPharmacyTest, unknownKey)
{
    VsdmHmacKey newKeyPackage{'$', '1'};
    newKeyPackage.setPlainTextKey(keyPackage.plainTextKey());
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', newKeyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                      "Prüfung der HMAC-Sicherung).");
}


TEST_F(GetTasksByPharmacyTest, differentKey)
{
    const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
    keyPackage.setPlainTextKey(Base64::encode(key));
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                      "Anwesenheitsnachweis konnte nicht erfolgreich durchgeführt werden (Fehler bei "
                                      "Prüfung der HMAC-Sicherung).");
}
// GEMREQ-end A_23456-01#test

TEST_F(GetTasksByPharmacyTest, pzTooShort)
{
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pzBin = Base64::decode(pz);
    pzBin.resize(46);
    auto pnw = createEncodedPnw(Base64::encode(pzBin));
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                          "Failed parsing PNW XML.", "Invalid size of Prüfziffer");
}

TEST_F(GetTasksByPharmacyTest, kvnrMismatch)
{
    auto pz = makePzV1(model::Kvnr("Y123456788"), model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(callHandler(pnw, serverResponse, kvnr), HttpStatus::KvnrMismatch,
                                      "KVNR Überprüfung fehlgeschlagen.");
}

TEST_F(GetTasksByPharmacyTest, pzTooLong)
{
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pzBin = Base64::decode(pz);
    pzBin.push_back('A');
    auto pnw = createEncodedPnw(Base64::encode(pzBin));
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden,
                                          "Failed parsing PNW XML.", "Invalid size of Prüfziffer");
}

TEST_F(GetTasksByPharmacyTest, invalidXml)
{
    std::string_view pnwData = "abc12345";
    const auto gzippedPnw = Deflate().compress(pnwData, Compression::DictionaryUse::Undefined);
    auto pnw = Base64::encode(gzippedPnw);
    ServerResponse serverResponse;

    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
        callHandler(pnw, serverResponse, kvnr), HttpStatus::Forbidden, "Failed parsing PNW XML.",
        "xml could not be parsed, error 4, at line 1, message: Start tag expected, '<' not found");
}

class GetTaskByIdByPharmacyTest : public EndpointHandlerTest
{
protected:
    static constexpr const char accessCode[] = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    static constexpr const char secret[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    static constexpr const char telematikID[] = "3-SMC-B-Testkarte-883110000120312";
    void callHandler(const model::PrescriptionId& prescriptionId, const std::string& telematikId,
                     std::optional<std::string> accessCode, std::optional<std::string> secret, ServerResponse& serverResponse)
    {
        using namespace std::string_literals;
        GetTaskHandler handler({});
        const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke(telematikId);
        Header requestHeader{HttpMethod::GET, "/Task/" + prescriptionId.toString(), 0, {}, HttpStatus::Unknown};
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setAccessToken(jwtPharmacy);
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        if (accessCode)
        {
            serverRequest.setQueryParameters(
                {{"ac"s, std::move(accessCode.value())}});
        }
        if (secret) {
            serverRequest.setQueryParameters(
                {{"secret"s, std::move(secret.value())}});
        }
        // store the unescaped string, the real handler also gets the the unescaped string
        AccessLog accessLog;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        handler.handleRequest(sessionContext);
    }

    void closeTask(const model::PrescriptionId& prescriptionId)
    {
        CloseTaskHandler handler({});
        const Header requestHeader{HttpMethod::POST,
                            "/Task/" + prescriptionId.toString() + "/$close/",
                            0,
                            {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                            HttpStatus::Unknown};

        ServerRequest serverRequest{requestHeader};
        serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtApotheke(telematikID));
        serverRequest.setQueryParameters({{"secret", secret}});
        serverRequest.setPathParameters({"id"}, {prescriptionId.toString()});
        serverRequest.setBody(ResourceTemplates::dispenseOrCloseTaskBodyXml({
            .profileType = model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input,
            .version = ResourceTemplates::Versions::GEM_ERP_current(),
            .medicationDispenses = {{.prescriptionId = prescriptionId, .telematikId = telematikID}},
        }));
        AccessLog accessLog;
        ServerResponse serverResponse;
        SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    }
};


TEST_F(GetTaskByIdByPharmacyTest, recoverSecret)
{
    A_24176.test("Calling Pharmacy is owner");
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(model::PrescriptionId::fromDatabaseId(
                                    model::PrescriptionType::apothekenpflichigeArzneimittel, 4715),
                                telematikID, accessCode, std::nullopt, serverResponse););
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    std::optional<model::Bundle> bundle;
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const gsl::not_null repoView = fhirInstance.structureRepository(model::Timestamp::now())
    .match(backend, std::string{model::resource::structure_definition::task},
           ResourceTemplates::Versions::GEM_ERP_current());
    auto factory = model::ResourceFactory<model::Bundle>::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator(), {});
    auto validationResults = factory.validateGeneric(*repoView, {}, {});
    ASSERT_TRUE(validationResults.highestSeverity() < fhirtools::Severity::error) << serverResponse.getBody();
    ASSERT_NO_THROW(bundle.emplace(std::move(factory).getNoValidation()));
    auto tasks = bundle->getResourcesByType<model::Task>();
    ASSERT_EQ(tasks.size(), 1);
    A_24179.test("Returned Task has secret");
    auto taskSecret = tasks[0].secret();
    ASSERT_TRUE(taskSecret.has_value());
    EXPECT_EQ(*taskSecret, secret);

    A_24179.test("Returned Bundle contains prescription");
    auto prescriptionBinaryUuid = tasks[0].healthCarePrescriptionUuid();
    ASSERT_TRUE(prescriptionBinaryUuid.has_value());
    auto prescriptionBinary = bundle->getResourcesByType<model::Binary>();
    ASSERT_EQ(prescriptionBinary.size(), 1) << bundle->serializeToJsonString();
    ASSERT_EQ(prescriptionBinary[0].id(), *prescriptionBinaryUuid);
}


TEST_F(GetTaskByIdByPharmacyTest, otherOwner)
{
    A_24176.test("Calling Pharmacy is not owner");
    ServerResponse serverResponse;
    try
    {
        callHandler(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4716),
            "3-SMC-B-Testkarte-045903495834092",// differs from Task.owner
            accessCode, std::nullopt, serverResponse);
        ADD_FAILURE() << "expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_EQ(ex.status(), HttpStatus::PreconditionFailed);
    }
    catch (const std::exception& ex)
    {
        FAIL() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}

TEST_F(GetTaskByIdByPharmacyTest, taskHasNoOwner)
{
    A_24176.test("Task of previous Software has no owner");
    ServerResponse serverResponse;
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571);
    ResourceTemplates::TaskOptions taskOpts{
        .taskType = ResourceTemplates::TaskType::InProgress,
        .prescriptionId = taskId,
    };
    auto doc = model::NumberAsStringParserDocument::fromJson(ResourceTemplates::taskJson(taskOpts));
    static const rapidjson::Pointer owner{"/owner"};
    owner.Erase(doc);
    mockDatabase->insertTask(model::Task::fromJson(doc));
    try
    {
        callHandler(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571),
            "3-SMC-B-Testkarte-045903495834092", accessCode, std::nullopt, serverResponse);
        ADD_FAILURE() << "expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_EQ(ex.status(), HttpStatus::PreconditionFailed);
    }
    catch (const std::exception& ex)
    {
        FAIL() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}

TEST_F(GetTaskByIdByPharmacyTest, wrongAccessCode)
{
    A_24177.test("accesscode is checked");
    ServerResponse serverResponse;
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571);
    ResourceTemplates::TaskOptions taskOpts{
        .taskType = ResourceTemplates::TaskType::InProgress,
        .prescriptionId = taskId,
    };
    auto doc = model::NumberAsStringParserDocument::fromJson(ResourceTemplates::taskJson(taskOpts));
    static const rapidjson::Pointer owner{"/owner"};
    owner.Erase(doc);
    mockDatabase->insertTask(model::Task::fromJson(doc));
    try
    {
        callHandler(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571),
            telematikID, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
            std::nullopt, serverResponse);
        ADD_FAILURE() << "expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_EQ(ex.status(), HttpStatus::Forbidden);
    }
    catch (const std::exception& ex)
    {
        FAIL() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}

TEST_F(GetTaskByIdByPharmacyTest, wrongTaskStatus)
{
    A_24178.test("status in progress");
    ServerResponse serverResponse;
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571);
    ResourceTemplates::TaskOptions taskOpts{
        .taskType = ResourceTemplates::TaskType::InProgress,
        .prescriptionId = taskId,
    };
    auto doc = model::NumberAsStringParserDocument::fromJson(ResourceTemplates::taskJson(taskOpts));
    rapidjson::Pointer statusPtr{"/status"};
    statusPtr.Set(doc, "$ready");
    mockDatabase->insertTask(model::Task::fromJson(doc));
    try
    {
        callHandler(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 234571),
            telematikID, accessCode, std::nullopt, serverResponse);
        ADD_FAILURE() << "expected ErpException";
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.status(), HttpStatus::PreconditionFailed);
        EXPECT_STREQ(ex.what(), "Task not in-progress.");
    }
    catch (const std::exception& ex)
    {
        FAIL() << "Unexpected exception: " << util::demangle(typeid(ex).name()) << ": " << ex.what();
    }
}

TEST_F(GetTaskByIdByPharmacyTest, accessCodeAndSecret)
{
    const auto prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    ASSERT_NO_FATAL_FAILURE(closeTask(prescriptionId));
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(prescriptionId, telematikID, accessCode, secret, serverResponse););
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    std::optional<model::Bundle> bundle;
    ASSERT_NO_THROW(bundle.emplace(
        testutils::getValidatedErxReceiptBundle<model::Bundle>(serverResponse.getBody(), model::ProfileType::fhir)));
    auto receiptBundle = bundle->getResourcesByType<model::ErxReceipt>();
    ASSERT_EQ(receiptBundle.size(), 1) << serverResponse.getBody();
    auto profileName = receiptBundle.at(0).getProfileName();
    ASSERT_TRUE(profileName.has_value());
    std::string expectedProfileName{model::resource::structure_definition::receipt};
    expectedProfileName += '|';
    expectedProfileName += to_string(ResourceTemplates::Versions::GEM_ERP_current());
    EXPECT_EQ(*profileName, expectedProfileName);
}

class GetTaskByIdByPharmacyTestErp17667 : public GetTasksByPharmacyTest
{
public:
    class MockDatabaseErp17667 : public MockDatabase
    {
    public:
        using MockDatabase::MockDatabase;
        std::vector<db_model::Task> retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                               const std::optional<UrlArguments>& search) override
        {
            auto res = MockDatabase::retrieveAll160TasksWithAccessCode(kvnrHashed, search);
            if (! res.empty())
            {
                // force race condition:
                MockDatabase::updateTaskClearPersonalData(res[0].prescriptionId, model::Task::Status::cancelled,
                                                          model::Timestamp::now(), model::Timestamp::now());
            }
            return res;
        }
    };

    GetTaskByIdByPharmacyTestErp17667()
        : mEnvGuard("TEST_USE_POSTGRES", "false")
        , mErp17667ServiceContext(
              StaticData::makePcServiceContext([this](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
                  // the modified mock DB changes the status of the task in the middle of the request.
                  return createDatabaseErp17667(hsmPool, keyDerivation);
              }))
    {
    }

    void SetUp() override
    {
        GetTasksByPharmacyTest::SetUp();
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);

        keyPackage.setPlainTextKey(Base64::encode(key));

        auto hsmPoolSession = mErp17667ServiceContext.getHsmPool().acquire();
        auto& hsmSession = hsmPoolSession.session();
        const ErpVector vsdmKeyData = ErpVector::create(keyPackage.serializeToString());
        ErpBlob vsdmKeyBlob = hsmSession.wrapRawPayload(vsdmKeyData, 0);

        auto& vsdmBlobDb = mErp17667ServiceContext.getVsdmKeyBlobDatabase();
        VsdmKeyBlobDatabase::Entry blobEntry;
        blobEntry.operatorId = keyPackage.operatorId();
        blobEntry.version = keyPackage.version();
        blobEntry.createdDateTime = model::Timestamp::now().toChronoTimePoint();
        blobEntry.blob = vsdmKeyBlob;
        vsdmBlobDb.storeBlob(std::move(blobEntry));
    }

    std::unique_ptr<DatabaseFrontend> createDatabaseErp17667(HsmPool& hsmPool, KeyDerivation& keyDerivation)
    {
        mockDatabase = std::make_unique<MockDatabaseErp17667>(hsmPool);
        mockDatabase->fillWithStaticData();
        auto md = std::make_unique<MockDatabaseProxy>(*mockDatabase);
        return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
    }

    EnvironmentVariableGuard mEnvGuard;
    PcServiceContext mErp17667ServiceContext;
};

TEST_F(GetTaskByIdByPharmacyTestErp17667, erp17667TaskStatusDataRace)
{
    GetAllTasksHandler handler({});

    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(jwtPharmacy);
    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    serverRequest.setQueryParameters({{"pnw", pnw}, {"kvnr", kvnr.id()}});
    AccessLog accessLog;
    ServerResponse serverResponse;
    SessionContext sessionContext{mErp17667ServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    handler.handleRequest(sessionContext);
}

TEST_F(GetTasksByPharmacyTest, Paging)
{
    const auto expiry = model::Timestamp::now() + 48h;
    const int64_t databaseId = 202830;
    for (int64_t i = 0; i < 30; i++)
    {
        insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
            ResourceTemplates::TaskType::Ready,
            databaseId + i, expiry
        );
    }
    {
        insertTask(model::PrescriptionType::direkteZuweisung,
            ResourceTemplates::TaskType::Ready,
            databaseId + 30, expiry
        );
        insertTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
            ResourceTemplates::TaskType::InProgress,
            databaseId + 31, expiry
        );
        insertTask(model::PrescriptionType::direkteZuweisung,
            ResourceTemplates::TaskType::InProgress,
            databaseId + 32, expiry
        );
        insertTask(model::PrescriptionType::direkteZuweisung,
            ResourceTemplates::TaskType::Completed,
            databaseId + 33, expiry
        );
    }

    auto pz = makePzV1(kvnr, model::Timestamp::now(), 'U', keyPackage);
    auto pnw = createEncodedPnw(pz);
    ServerResponse serverResponse;
    ASSERT_NO_THROW(callHandler(pnw, serverResponse, kvnr, hcv, 10));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    model::Bundle taskBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody());
    EXPECT_EQ(taskBundle.getBundleType(), model::BundleType::searchset);
    const auto tasks = taskBundle.getResourcesByType<model::Task>("Task");
    EXPECT_EQ(tasks.size(), 10);

    const auto self = "/Task?pnw=" + UrlHelper::escapeUrl(pnw) + "&kvnr=" + kvnr.id() +
                      "&hcv=" + Base64::toBase64Url(Base64::encode(hcv)) + "&_sort=authored-on";

    validateLink(taskBundle, model::Link::Type::Self, self + "&_count=10&__offset=0");
    validateLink(taskBundle, model::Link::Type::Next, self + "&_count=10&__offset=10");
    validateLink(taskBundle, model::Link::Type::Last, self + "&_count=10&__offset=30");
}
