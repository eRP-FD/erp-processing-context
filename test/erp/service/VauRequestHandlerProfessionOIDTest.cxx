/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpProcessingContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ProfessionOid.hxx"

#include "shared/crypto/Certificate.hxx"
#include "shared/util/String.hxx"


#include "mock/crypto/MockCryptography.hxx"

#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <chrono>

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif

class VauRequestHandlerProfessionOIDTest : public ServerTestBase
{
public:
    ClientRequest makeEncryptedRequest (const HttpMethod method, const std::string_view endpoint, const JWT& jwt1)
    {
        // Create the inner request
        ClientRequest request(Header(
                method,
                static_cast<std::string>(endpoint),
                Header::Version_1_1,
                { getAuthorizationHeaderForJwt(jwt1) },
                HttpStatus::Unknown),
                "");

        // Encrypt with TEE protocol.

        ClientRequest encryptedRequest(Header(
                HttpMethod::POST,
                "/VAU/0",
                Header::Version_1_1,
                {},
                HttpStatus::Unknown),
                teeProtocol.createRequest(
                        MockCryptography::getEciesPublicKeyCertificate(),
                        request,
                        jwt1));
        encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
        return encryptedRequest;
    }

    // GEMREQ-start testEndpoint
    void testEndpoint (const HttpMethod method, const std::string_view endpoint, const JWT& jwt1, HttpStatus expected)
    {
        // Send the request.
        auto response = mClient.send(makeEncryptedRequest(method, endpoint, jwt1));

        // Verify the outer response
        ASSERT_EQ(response.getHeader().status(), HttpStatus::OK)
                                << "header status was " << toString(response.getHeader().status());
        if (response.getHeader().hasHeader(Header::ContentType))
        {
            ASSERT_EQ(response.getHeader().header(Header::ContentType), "application/octet-stream");
        }

        // Verify the inner response;
        const ClientResponse decryptedResponse = teeProtocol.parseResponse(response);
        ASSERT_EQ(decryptedResponse.getHeader().status(), expected)
                                << "professionOID: " << jwt1.stringForClaim(JWT::professionOIDClaim).value_or("xxx")
                                << " configuration is wrong for endpoint " << endpoint;
    }
    // GEMREQ-end testEndpoint


    JWT jwtWithInvalidProfessionOID ()
    {
        rapidjson::Document document;
        std::string jwtClaims = R"({
            "acr": "gematik-ehealth-loa-high",
            "aud": "https://gematik.erppre.de/",
            "exp": ##EXP##,
            "family_name": "Nachname",
            "given_name": "Vorname",
            "iat": ##IAT##,
            "idNummer": "recipient",
            "iss": "https://idp1.telematik.de/jwt",
            "jti": "<IDP>_01234567890123456789",
            "nonce": "fuu bar baz",
            "organizationName": "Organisation",
            "professionOID": "123",
            "sub": "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"
        })";
        // Token will expire from execution time T + 5 minutes.
        jwtClaims = String::replaceAll(jwtClaims, "##EXP##", std::to_string( +60l * 5l + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
        // Token is issued  at execution time T - 1 minutes.
        jwtClaims = String::replaceAll(jwtClaims, "##IAT##", std::to_string( -60 + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
        document.Parse(jwtClaims);
        return mJwtBuilder.getJWT(document);
    }

    VauRequestHandlerProfessionOIDTest ()
            : ServerTestBase(true)

            , jwtVersicherter(jwtWithProfessionOID(profession_oid::oid_versicherter))
            , jwtOeffentliche_apotheke(jwtWithProfessionOID(profession_oid::oid_oeffentliche_apotheke))
            , jwtKrankenhausapotheke(jwtWithProfessionOID(profession_oid::oid_krankenhausapotheke))
            , jwtArzt(jwtWithProfessionOID(profession_oid::oid_arzt))
            , jwtZahnArzt(jwtWithProfessionOID(profession_oid::oid_zahnarzt))
            , jwtPraxisArzt(jwtWithProfessionOID(profession_oid::oid_praxis_arzt))
            , jwtZahnArztPraxis(jwtWithProfessionOID(profession_oid::oid_zahnarztpraxis))
            , jwtPraxisPsychotherapeut(jwtWithProfessionOID(profession_oid::oid_praxis_psychotherapeut))
            , jwtKrankenhaus(jwtWithProfessionOID(profession_oid::oid_krankenhaus))
            , jwtBundeswehrapotheke(jwtWithProfessionOID(profession_oid::oid_bundeswehrapotheke))
            , teeProtocol()
            , mClient(createClient())
            , taskId(model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4711).toString())
            , taskIdNotFound(model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 999999).toString())
    {
    }

    void SetUp (void) override
    {
        ASSERT_NO_FATAL_FAILURE(ServerTestBase::SetUp());
        ASSERT_NO_FATAL_FAILURE(Fhir::instance());
    }

    std::unique_ptr<Database> createDatabaseFrontend()
    {
        auto databaseFrontend = ServerTestBase::createDatabase();
        dynamic_cast<MockDatabase&>(databaseFrontend->getBackend()).fillWithStaticData();
        return databaseFrontend;
    }


    JWT jwtVersicherter;
    JWT jwtOeffentliche_apotheke;
    JWT jwtKrankenhausapotheke;
    JWT jwtArzt;
    JWT jwtZahnArzt;
    JWT jwtPraxisArzt;
    JWT jwtZahnArztPraxis;
    JWT jwtPraxisPsychotherapeut;
    JWT jwtKrankenhaus;
    JWT jwtBundeswehrapotheke;

    ClientTeeProtocol teeProtocol;
    HttpsClient mClient;

    std::string taskId;
    std::string taskIdNotFound;
};

TEST_F(VauRequestHandlerProfessionOIDTest, GetAllTasksSuccess)
{
    A_21558_01.test("Valid professionOID claim in JWT");
    testEndpoint(HttpMethod::GET, "/Task", jwtVersicherter, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, "/Task", jwtOeffentliche_apotheke, HttpStatus::Forbidden);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetAllTasksForbidden)
{
    A_21558_01.test("Invalid professionOID claim in JWT");
    testEndpoint(HttpMethod::GET, "/Task", jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, "/Task", jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetTaskSuccess)
{
    A_19113_01.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound;
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint + "?secret=caffe", jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint + "?secret=caffe", jwtKrankenhausapotheke, HttpStatus::NotFound);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetTaskForbidden)
{
    A_19113_01.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId;
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskCreateSuccess)
{
    A_19018.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Task/$create";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::BadRequest);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskCreateForbidden)
{
    A_19018.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Task/$create";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskActivateSuccess)
{
    A_19022.test("Valid professionOID claim in JWT");
    // We want to trigger a NotFound, that can be distinguished from Forbidden.
    // Not found means in this case that the professionOID test passes, but the task could not be found.
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$activate";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskActivateForbidden)
{
    A_19022.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$activate";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskAcceptSuccess)
{
    A_19166.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$accept?ac=access_code";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskAcceptForbidden)
{
    A_19166.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$accept";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskRejectSuccess)
{
    A_19170_01.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$reject";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskRejectForbidden)
{
    A_19170_01.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$reject";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

// GEMREQ-start A_24279
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskDispenseSuccess)
{
    A_24279.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$dispense";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskDispenseForbidden)
{
    A_24279.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$dispense";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_24279

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskCloseSuccess)
{
    A_19230.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$close?secret=secret";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskCloseForbidden)
{
    A_19230.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$close";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskAbortSuccess)
{
    A_19026.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskIdNotFound + "/$abort";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::NotFound);
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostTaskAbortForbidden)
{
    A_19026.test("Invalid professionOID claim in JWT");
    const std::string endpoint = "/Task/" + taskId + "/$abort";
    testEndpoint(HttpMethod::POST, endpoint, jwtBundeswehrapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

// GEMREQ-start A_19405
TEST_F(VauRequestHandlerProfessionOIDTest, GetAllMedicationDispensesSuccess)
{
    A_19405.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/MedicationDispense";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetAllMedicationDispensesForbidden)
{
    A_19405.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/MedicationDispense";
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_19405

TEST_F(VauRequestHandlerProfessionOIDTest, GetMedicationDispenseSuccess)
{
    A_19405.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/MedicationDispense/160.000.000.004.711.86";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetMedicationDispenseForbidden)
{
    A_19405.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/MedicationDispense/160.000.000.004.711.86";
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetAllCommunicationSuccess)
{
    A_19446_01.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::OK);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetAllCommunicationForbidden)
{
    A_19446_01.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication";
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetCommunicationSuccess)
{
    A_19446_01.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetCommunicationForbidden)
{
    A_19446_01.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostCommunicationSuccess)
{
    A_19446_01.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::BadRequest);
}
TEST_F(VauRequestHandlerProfessionOIDTest, PostCommunicationForbidden)
{
    A_19446_01.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication";
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, DeleteCommunicationSuccess)
{
    A_19446_01.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::NotFound);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, DeleteCommunicationForbidden)
{
    A_19446_01.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Communication/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetAllAuditEventsSuccess)
{
    A_19395.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/AuditEvent";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetAllAuditEventsForbidden)
{
    A_19395.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/AuditEvent";
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetAuditEventSuccess)
{
    A_19395.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/AuditEvent/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::NotFound);
}
TEST_F(VauRequestHandlerProfessionOIDTest, GetAuditEventForbidden)
{
    A_19395.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/AuditEvent/47110000-0000-0000-0000-000000000000";
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetDeviceSuccess)
{
    const std::string_view endpoint = "/Device";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::OK);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetMetadataSuccess)
{
    const std::string_view endpoint = "/metadata";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::OK);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::OK);
}

// GEMREQ-start A_22113
TEST_F(VauRequestHandlerProfessionOIDTest, DeleteChargeItemSuccess)
{
    A_22113.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskIdNotFound;
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::NotFound);
}

TEST_F(VauRequestHandlerProfessionOIDTest, DeleteChargeItemForbidden)
{
    A_22113.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskId;
    testEndpoint(HttpMethod::DELETE, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22113

// GEMREQ-start A_22118
TEST_F(VauRequestHandlerProfessionOIDTest, GetAllChargeItemsSuccess)
{
    A_22118.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetAllChargeItemsForbidden)
{
    A_22118.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem";
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
}
// GEMREQ-end A_22118

// GEMREQ-start A_22124
TEST_F(VauRequestHandlerProfessionOIDTest, GetChargeItemSuccess)
{
    A_22124.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskIdNotFound;
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::NotFound);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::NotFound);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetChargeItemForbidden)
{
    A_22124.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskId;
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22124

// GEMREQ-start A_22129
TEST_F(VauRequestHandlerProfessionOIDTest, PostChargeItemSuccess)
{
    A_22129.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostChargeItemForbidden)
{
    A_22129.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22129

// GEMREQ-start A_22144
TEST_F(VauRequestHandlerProfessionOIDTest, PutChargeItemSuccess)
{
    A_22144.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskIdNotFound;
    testEndpoint(HttpMethod::PUT, endpoint, jwtOeffentliche_apotheke, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::PUT, endpoint, jwtKrankenhausapotheke, HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PutChargeItemForbidden)
{
    A_22144.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskId;
    testEndpoint(HttpMethod::PUT, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PUT, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22144

TEST_F(VauRequestHandlerProfessionOIDTest, DeleteConsentMethodNotAllowed)
{
    const std::string_view endpoint = "/Consent";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::MethodNotAllowed);
}

TEST_F(VauRequestHandlerProfessionOIDTest, DeleteConsentUnknownCategory)
{
    const std::string_view endpoint = "/Consent/?category=unknown";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::BadRequest);
}

// GEMREQ-start A_22875
TEST_F(VauRequestHandlerProfessionOIDTest, PatchChargeItemSuccess)
{
    A_22875.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskIdNotFound;
    testEndpoint(HttpMethod::PATCH, endpoint, jwtVersicherter, HttpStatus::NotFound);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PatchChargeItemForbidden)
{
    A_22875.test("Valid professionOID claim in JWT");
    const std::string endpoint = "/ChargeItem/" + taskId;
    testEndpoint(HttpMethod::PATCH, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::PATCH, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22875

// GEMREQ-start A_22155
TEST_F(VauRequestHandlerProfessionOIDTest, DeleteConsentForbidden)
{
    A_22155.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent/?category=CHARGCONS";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::DELETE, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}

TEST_F(VauRequestHandlerProfessionOIDTest, DeleteConsentSuccess)
{
    A_22155.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent/?category=CHARGCONS";
    testEndpoint(HttpMethod::DELETE, endpoint, jwtVersicherter, HttpStatus::NotFound);
}
// GEMREQ-end A_22155

// GEMREQ-start A_22159
TEST_F(VauRequestHandlerProfessionOIDTest, GetConsentSuccess)
{
    A_22159.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent/";
    testEndpoint(HttpMethod::GET, endpoint, jwtVersicherter, HttpStatus::OK);
}

TEST_F(VauRequestHandlerProfessionOIDTest, GetConsentForbidden)
{
    A_22159.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent";
    testEndpoint(HttpMethod::GET, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::GET, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22159

// GEMREQ-start A_22161
TEST_F(VauRequestHandlerProfessionOIDTest, PostConsentSuccess)
{
    A_22161.test("Valid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent/";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::BadRequest);
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostConsentForbidden)
{
    A_22161.test("Invalid professionOID claim in JWT");
    const std::string_view endpoint = "/Consent";
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
}
// GEMREQ-end A_22161

TEST_F(VauRequestHandlerProfessionOIDTest, PostSubscriptionSuccess)
{
    const std::string_view endpoint = "/Subscription";

    A_22362.test("Only registered professionOIDs are allowed to call this service.");
    // Since content type is missing in the header, a BadRequest will follow. The oid check is passed, though.
    testEndpoint(HttpMethod::POST, endpoint, jwtOeffentliche_apotheke, HttpStatus::BadRequest);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhausapotheke, HttpStatus::BadRequest);
    A_22362.finish();
}

TEST_F(VauRequestHandlerProfessionOIDTest, PostSubscriptionForbidden)
{
    A_22362.test("All other professionOIDs are forbidden for this service.");
    const std::string_view endpoint = "/Subscription";
    testEndpoint(HttpMethod::POST, endpoint, jwtVersicherter, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisArzt, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtZahnArztPraxis, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtPraxisPsychotherapeut, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtKrankenhaus, HttpStatus::Forbidden);
    testEndpoint(HttpMethod::POST, endpoint, jwtWithInvalidProfessionOID(), HttpStatus::Forbidden);
    A_22362.finish();
}
