/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ServerTestBase.hxx"

#include "mock/hsm/MockBlobCache.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/StaticData.hxx"
#include "test_config.h"
#include "erp/ErpProcessingContext.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/Patient.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"

#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"

#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/StaticData.hxx"


#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif

using namespace model;


static constexpr const char* AuthorizationBearerPrefix = "Bearer ";

template<class T> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const T& jwt1);

template<> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const JWT& jwt1)
{
    return { Header::Authorization, AuthorizationBearerPrefix + jwt1.serialize() };
}

template<> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const std::string& jwt1)
{
    return { Header::Authorization, AuthorizationBearerPrefix + jwt1 };
}

std::unique_ptr<RedisInterface> createRedisInstance (void)
{
    // Check if a switch between redis interfaces is required (mock vs real client.)
    return std::make_unique<MockRedisStore>();
}

ServerTestBase::ServerTestBase(const bool forceMockDatabase)
    : mServer(),
      mJwt(),
      mTeeProtocol(),
      mHasPostgresSupport(!forceMockDatabase && isPostgresEnabled()),
      mJwtBuilder(MockCryptography::getIdpPrivateKey()),
      mPharmacy(mJwtBuilder.makeJwtApotheke().stringForClaim(JWT::idNumberClaim).value()),
      mDoctor(mJwtBuilder.makeJwtArzt().stringForClaim(JWT::idNumberClaim).value()),
      mConnection()
{
}

ServerTestBase::~ServerTestBase() = default;

void ServerTestBase::startServer (void)
{
    if (mServer != nullptr)
    {
        // Server already created and assumed running. Nothing more to do.
        return;
    }

    mJwt = std::make_unique<JWT>( mJwtBuilder.makeJwtVersicherter(InsurantF) );

    RequestHandlerManager<PcServiceContext> handlers;
    RequestHandlerManager<PcServiceContext> secondaryHandlers;
    addAdditionalPrimaryHandlers(handlers);
    addAdditionalSecondaryHandlers(secondaryHandlers); // Allow derived test classes to add additional handlers.
    ErpProcessingContext::addPrimaryEndpoints(handlers, std::move(secondaryHandlers));

    auto hsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(
            std::make_unique<HsmMockClient>(),
            MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory());
    mMockDatabase = std::make_unique<MockDatabase>(*hsmPool);

    // Create service context without TslManager, the TslManager functionality is skipped for these tests.
    auto serviceContext = std::make_unique<PcServiceContext>(
        Configuration::instance(),
        createDatabaseFactory(),
        createRedisInstance(),
        std::move(hsmPool),
        StaticData::getJsonValidator(),
        StaticData::getXmlValidator());
    initializeIdp(serviceContext->idp);
    mServer = std::make_unique<HttpsServer<PcServiceContext>>(
        "0.0.0.0",
        static_cast<uint16_t>(9999),
        std::move(handlers),
        std::move(serviceContext));
    mServer->serve(serverThreadCount);
}


void ServerTestBase::SetUp (void)
{
    try
    {
        startServer();
    }
    catch(std::exception& exc)
    {
        LOG(ERROR) << "starting the server failed, reason: " << exc.what();
        FAIL();
    }
    catch(...)
    {
        LOG(ERROR) << "starting the server failed";
        FAIL();
    }

    if (mHasPostgresSupport)
    {
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.communication");
        deleteTxn.exec("DELETE FROM erp.task");
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.commit();
    }
}


void ServerTestBase::TearDown (void)
{
    Environment::unset("ERP_IDP_PUBLIC_KEY");

    if (mServer != nullptr)
    {
        mServer->shutDown();
        mServer.reset();
    }

    if (mHasPostgresSupport)
    {
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.communication");
        deleteTxn.exec("DELETE FROM erp.task");
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.commit();
    }
}


HttpsClient ServerTestBase::createClient (void)
{
    return HttpsClient("127.0.0.1", 9999, 30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
}


Header ServerTestBase::createPostHeader (const std::string& path, const std::optional<const JWT>& jwtToken) const
{
    return Header(
        HttpMethod::POST,
        std::string(path),
        Header::Version_1_1,
        { {Header::Authorization, "Bearer " + std::string(jwtToken.has_value() ? jwtToken->serialize() : mJwt->serialize())}},
        HttpStatus::Unknown);
}


Header ServerTestBase::createGetHeader (const std::string& path, const std::optional<const JWT>& jwtToken) const
{
    return Header(
        HttpMethod::GET,
        std::string(path),
        Header::Version_1_1,
        { {Header::Authorization, "Bearer " + std::string(jwtToken.has_value() ? jwtToken->serialize() : mJwt->serialize())} },
        HttpStatus::Unknown);
}


Header ServerTestBase::createDeleteHeader(const std::string& path, const std::optional<const JWT>& jwtToken) const
{
    Header header(
        HttpMethod::DELETE,
        std::string(path),
        Header::Version_1_1,
        { {Header::Authorization, "Bearer " + std::string(jwtToken.has_value() ? jwtToken->serialize() : mJwt->serialize())} },
        HttpStatus::Unknown);
    return header;
}

ClientRequest ServerTestBase::encryptRequest (const ClientRequest& innerRequest, const std::optional<const JWT>& jwtToken)
{
    ClientRequest outerRequest(
        Header(
            HttpMethod::POST,
            "/VAU/0",
            Header::Version_1_1,
            {{Header::XRequestId, "the-request-id"}},
            HttpStatus::Unknown),
        mTeeProtocol.createRequest(
            MockCryptography::getEciesPublicKeyCertificate(),
            innerRequest,
            jwtToken.has_value() ? *jwtToken : *mJwt));
    outerRequest.setHeader(Header::ContentType, MimeType::binary);
    return outerRequest;
}


ClientResponse ServerTestBase::verifyOuterResponse (const ClientResponse& outerResponse)
{
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK)
                    << "header status was " << toString(outerResponse.getHeader().status());
    EXPECT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
    EXPECT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), MimeType::binary);

    return mTeeProtocol.parseResponse(outerResponse);
}


void ServerTestBase::verifyGenericInnerResponse (
    const ClientResponse& innerResponse,
    const HttpStatus expectedStatus,
    const std::string& expectedMimeType)
{
    ASSERT_EQ(innerResponse.getHeader().status(), expectedStatus);
    if (expectedStatus == HttpStatus::OK)
    {
        ASSERT_EQ(innerResponse.getHeader().header(Header::ContentType), expectedMimeType);
        ASSERT_TRUE(innerResponse.getHeader().hasHeader(Header::ContentLength));
        ASSERT_EQ(innerResponse.getHeader().header(Header::ContentLength).value(), std::to_string(innerResponse.getBody().size()));
    }
}

Database::Factory ServerTestBase::createDatabaseFactory (void)
{
    if (mHasPostgresSupport)
    {
        return [](HsmPool& hsmPool, KeyDerivation& keyDerivation){
            return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), hsmPool, keyDerivation); };
    }
    else
    {
        return [this](HsmPool& hsmPool, KeyDerivation& keyDerivation){
                return std::make_unique<DatabaseFrontend>(
                    std::make_unique<MockDatabaseProxy>(*mMockDatabase), hsmPool, keyDerivation);
            };
    }
}

std::unique_ptr<Database> ServerTestBase::createDatabase()
{
    auto serviceContext = mServer->serviceContext();
    return createDatabaseFactory()(serviceContext->getHsmPool(), serviceContext->getKeyDerivation());
}


pqxx::connection& ServerTestBase::getConnection()
{
    Expect(mHasPostgresSupport, "can not return Postgres connection because postgres support is disabled");
    if ( ! mConnection)
    {
        mConnection = std::make_unique<pqxx::connection>(PostgresBackend::defaultConnectString());
    }
    return *mConnection;
}


pqxx::work ServerTestBase::createTransaction()
{
    Expect(mHasPostgresSupport, "can not return Postgres transaction because postgres support is disabled");
    return pqxx::work{getConnection()};
}

JWT ServerTestBase::jwtWithProfessionOID(const std::string_view professionOID)
{
    rapidjson::Document document;
    std::string jwtClaims = R"({
            "acr": "gematik-ehealth-loa-high",
            "aud": "https://gematik.erppre.de/",
            "exp": ##EXP##,
            "family_name": "Nachname",
            "given_name": "Vorname",
            "iat": ##IAT##,
            "idNummer": "recipient0",
            "iss": "https://idp1.telematik.de/jwt",
            "jti": "<IDP>_01234567890123456789",
            "nonce": "fuu bar baz",
            "organizationName": "Organisation",
            "professionOID": "##PROFESSION-OID##",
            "sub": "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"
        })";

    jwtClaims = String::replaceAll(jwtClaims, "##PROFESSION-OID##", std::string(professionOID));
    // Token will expire from execution time T + 5 minutes.
    jwtClaims = String::replaceAll(jwtClaims, "##EXP##", std::to_string( +60 * 5 + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
    // Token is isseud  at execution time T - 1 minutes.
    jwtClaims = String::replaceAll(jwtClaims, "##IAT##", std::to_string( -60 + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
    document.Parse(jwtClaims);
    return mJwtBuilder.getJWT(document);
}


std::vector<model::Task> ServerTestBase::addTasksToDatabase(const std::vector<TaskDescriptor>& descriptors)
{
    std::vector<model::Task> tasks;
    for (const auto& descriptor : descriptors)
    {
        tasks.emplace_back(addTaskToDatabase(descriptor));
    }
    return tasks;
}

Task ServerTestBase::addTaskToDatabase(const TaskDescriptor& descriptor)
{
    if (descriptor.status == Task::Status::cancelled)
    {
        Fail("To cancel a task the task must have been created before");
    }

    Task task = createTask(descriptor.accessCode);

    if (descriptor.status == Task::Status::ready)
    {
        activateTask(task, descriptor.kvnrPatient);
    }
    else if (descriptor.status == Task::Status::inprogress)
    {
        activateTask(task, descriptor.kvnrPatient);
        acceptTask(task);
    }
    else if (descriptor.status == Task::Status::completed)
    {
        activateTask(task, descriptor.kvnrPatient);
        acceptTask(task);
        closeTask(task, descriptor.telematicIdPharmacy.has_value() ? descriptor.telematicIdPharmacy.value() : mPharmacy);
    }
    return task;
}

Task ServerTestBase::createTask(const std::string& accessCode)
{
    PrescriptionType prescriptionType = PrescriptionType::apothekenpflichigeArzneimittel;
    Task task(prescriptionType, accessCode);
    auto database = createDatabase();
    PrescriptionId prescriptionId = database->storeTask(task);
    task.setPrescriptionId(prescriptionId);
    database->commitTransaction();
    return task;
}

void ServerTestBase::activateTask(Task& task, const std::string& kvnrPatient)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    task.setKvnr(kvnrPatient);

    std::string prescriptionBundleXmlString =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/MedicationDispenseKbvBundle1.xml");

    prescriptionBundleXmlString =
        String::replaceAll(prescriptionBundleXmlString, "##PrescriptionId##", prescriptionId.toString());
    prescriptionBundleXmlString =
        String::replaceAll(prescriptionBundleXmlString, "##kvnrPatient##", kvnrPatient);

    Bundle prescriptionBundle =
        Bundle::fromXml(prescriptionBundleXmlString, *StaticData::getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle);

    const std::vector<Patient> patients = prescriptionBundle.getResourcesByType<Patient>("Patient");

    ASSERT_TRUE(patients.size() > 0);

    for (const auto& patient : patients)
    {
        ASSERT_EQ(patient.getKvnr(), kvnrPatient);
    }

    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setExpiryDate(Timestamp::now() + std::chrono::hours(24) * 92);
    task.setAcceptDate(Timestamp::now());
    task.setStatus(Task::Status::ready);
    task.updateLastUpdate();

    auto privKey = MockCryptography::getIdpPrivateKey();
    auto cert = MockCryptography::getIdpPublicKeyCertificate();
    CadesBesSignature cadesBesSignature{ cert, privKey, prescriptionBundleXmlString };
    std::string encodedPrescriptionBundleXmlString = Base64::encode(cadesBesSignature.get());
    const Binary healthCareProviderPrescriptionBinary(*task.healthCarePrescriptionUuid(), encodedPrescriptionBundleXmlString);

    auto database = createDatabase();
    database->activateTask(task, healthCareProviderPrescriptionBinary);
    database->commitTransaction();
}

void ServerTestBase::acceptTask(model::Task& task, const SafeString secret)
{
    task.setStatus(model::Task::Status::inprogress);
    task.setSecret(ByteHelper::toHex(secret));
    task.updateLastUpdate();

    auto database = createDatabase();
    database->updateTaskStatusAndSecret(task);
    database->commitTransaction();
}

MedicationDispense ServerTestBase::closeTask(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const std::optional<Timestamp>& medicationWhenPrepared)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    const Timestamp inProgessDate = task.lastModifiedDate();
    const Timestamp completedTimestamp = model::Timestamp::now();
    const std::string linkBase = "https://127.0.0.1:8080";
    const std::string authorIdentifier = linkBase + "/Device";
    Composition compositionResource(telematicIdPharmacy, inProgessDate, completedTimestamp, authorIdentifier);
    Device deviceResource;

    task.setReceiptUuid();
    task.setStatus(Task::Status::completed);
    task.updateLastUpdate();

    MedicationDispense medicationDispense =
        createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, medicationWhenPrepared);

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/",
        prescriptionId, compositionResource, authorIdentifier, deviceResource);

    auto database = createDatabase();
    database->updateTaskMedicationDispenseReceipt(task, medicationDispense, responseReceipt);
    database->deleteCommunicationsForTask(task.prescriptionId());
    database->commitTransaction();

    return medicationDispense;
}

void ServerTestBase::abortTask(Task& task)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    task.setStatus(model::Task::Status::cancelled);
    task.deleteKvnr();
    task.deleteInput();
    task.deleteOutput();
    task.deleteAccessCode();
    task.deleteSecret();
    task.updateLastUpdate();

    auto database = createDatabase();
    database->deleteCommunicationsForTask(prescriptionId);
    database->updateTaskClearPersonalData(task);
    database->commitTransaction();
}

MedicationDispense ServerTestBase::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::optional<Timestamp>& whenPrepared) const
{
    const std::string medicationDispenseString =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/fhir/conversion/medication_dispense.xml");

    MedicationDispense medicationDispense = MedicationDispense::fromXml(
            medicationDispenseString,
            *StaticData::getXmlValidator(),
            SchemaType::Gem_erxMedicationDispense);

    PrescriptionId prescriptionId = task.prescriptionId();
    std::string_view kvnrPatient = task.kvnr().value();

    medicationDispense.setId(prescriptionId);
    medicationDispense.setKvnr(kvnrPatient);
    medicationDispense.setTelematicId(telematicIdPharmacy);
    medicationDispense.setWhenHandedOver(whenHandedOver);

    if (whenPrepared.has_value())
    {
        medicationDispense.setWhenPrepared(whenPrepared.value());
    }

    return medicationDispense;
}

std::vector<Uuid> ServerTestBase::addCommunicationsToDatabase(const std::vector<CommunicationDescriptor>& descriptors)
{
    std::vector<Uuid> communicationIds;
    for (const auto& descriptor : descriptors)
    {
        communicationIds.emplace_back(addCommunicationToDatabase(descriptor).id().value());
    }
    return communicationIds;
}

model::Communication ServerTestBase::addCommunicationToDatabase(const CommunicationDescriptor& descriptor)
{
    auto builder = CommunicationJsonStringBuilder(descriptor.messageType);
    if (descriptor.prescriptionId.has_value())
    {
        builder.setPrescriptionId(descriptor.prescriptionId->toString());
    }
    if (!descriptor.sender.name.empty())
    {
        builder.setSender(descriptor.sender.role, descriptor.sender.name);
    }
    if (!descriptor.recipient.name.empty())
    {
        builder.setRecipient(descriptor.recipient.role, descriptor.recipient.name);
    }
    if (descriptor.contentString.has_value())
    {
        builder.setPayload(descriptor.contentString.value());
    }
    if (descriptor.sent.has_value())
    {
        builder.setTimeSent(descriptor.sent->toXsDateTime());
    }
    if (descriptor.received.has_value())
    {
        builder.setTimeReceived(descriptor.received->toXsDateTime());
    }
    if (descriptor.messageType == model::Communication::MessageType::InfoReq)
    {
        builder.setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de");
    }
    model::Communication communication = Communication::fromJson(builder.createJsonString());
    auto database = createDatabase();
    const auto communicationId = database->insertCommunication(communication);
    database->commitTransaction();
    communication.setId(communicationId.value());
    return communication;

}

void ServerTestBase::initializeIdp (Idp& idp)
{
    idp.setCertificate(Certificate(MockCryptography::getIdpPublicKeyCertificate()));
}

void ServerTestBase::writeCurrentTestOutputFile(
    const std::string& testOutput,
    const std::string& fileExtension,
    const std::string& marker)
{
    const ::testing::TestInfo* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string testName = testInfo->name();
    std::string testCaseName = testInfo->test_case_name();
    std::string fileName = testCaseName + "-" + testName;
    if (!marker.empty())
        fileName += marker;
    fileName += "." + fileExtension;
    FileHelper::writeFile(fileName, testOutput);
}

bool ServerTestBase::isPostgresEnabled()
{
    return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
}
