/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ServerTestBase.hxx"
#include "TestUtils.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "mock/client/TlsCertificateVerifierNoVerificationImplementation.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "shared/common/Constants.hxx"
#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/hsm/production/ProductionBlobDatabase.hxx"
#include "shared/model/Composition.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/Patient.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/erp/database/PostgresDatabaseTestFixture.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"


#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif

using namespace model;


static constexpr const char* AuthorizationBearerPrefix = "Bearer ";


std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const JWT& jwt1)
{
    return { Header::Authorization, AuthorizationBearerPrefix + jwt1.serialize() };
}

std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const std::string& jwt1)
{
    return { Header::Authorization, AuthorizationBearerPrefix + jwt1 };
}

bool HttpsReconnectingClient::testConnection()
{
    return mClient->testConnection();
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
    // init now, should avoid connection timeouts later
    (void)Fhir::instance();

    mJwt = std::make_unique<JWT>( mJwtBuilder.makeJwtVersicherter(InsurantF) );

    RequestHandlerManager handlers;
    RequestHandlerManager secondaryHandlers;
    addAdditionalPrimaryHandlers(handlers);
    addAdditionalSecondaryHandlers(secondaryHandlers); // Allow derived test classes to add additional handlers.
    ErpProcessingContext::addPrimaryEndpoints(handlers, std::move(secondaryHandlers));

    mHsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(
            std::make_unique<HsmMockClient>(),
            MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
    mMockDatabase = std::make_unique<MockDatabase>(*mHsmPool);

    auto factories = StaticData::makeMockFactories();
    factories.databaseFactory = createDatabaseFactory(&PostgresBackend::mainConnection);
    if (PostgresBackend::haveReadOnlyConnection())
    {
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
        {
            factories.readOnlyDatabaseFactory = createDatabaseFactory(&PostgresBackend::readOnlyConnection);
        }
        else
        {
            factories.readOnlyDatabaseFactory = factories.databaseFactory;
        }
    }
    factories.redisClientFactory = [](std::chrono::milliseconds){return createRedisInstance();};
    mContext = std::make_unique<PcServiceContext>(Configuration::instance(), std::move(factories));
    initializeIdp(mContext->idp);
    const auto& config = Configuration::instance();
    mServer = std::make_unique<HttpsServer>(
        "0.0.0.0",
        static_cast<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)),
        std::move(handlers),
        *mContext);
    mServer->serve(serverThreadCount, "admin-test");
}


void ServerTestBase::SetUp (void)
{
    try
    {
        startServer();
    }
    catch(const std::exception& exc)
    {
        TLOG(ERROR) << "starting the server failed, reason: " << exc.what();
        FAIL();
    }
    catch(...)
    {
        TLOG(ERROR) << "starting the server failed";
        FAIL();
    }

    if (mHasPostgresSupport)
    {
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.communication");
        for (auto prescriptionType : magic_enum::enum_values<model::PrescriptionType>())
        {
            deleteTxn.exec("DELETE FROM " + PostgresDatabaseTest::taskTableName(prescriptionType));
        }
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.exec("DELETE FROM erp.charge_item");
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
        deleteTxn.exec("DELETE FROM erp.task_169");
        deleteTxn.exec("DELETE FROM erp.task_200");
        deleteTxn.exec("DELETE FROM erp.task_209");
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.exec("DELETE FROM erp.charge_item");
        deleteTxn.commit();
    }
}


HttpsReconnectingClient ServerTestBase::createClient (void)
{
    const auto& config = Configuration::instance();
    return HttpsReconnectingClient(ConnectionParameters{
        .hostname = "127.0.0.1",
        .port = config.getStringValue(ConfigurationKey::ADMIN_SERVER_PORT),
        .connectionTimeout = std::chrono::seconds{30},
        .resolveTimeout = Constants::resolveTimeout,
        .tlsParameters = TlsConnectionParameters{
            .certificateVerifier = TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting()
        }
    });
}


template<typename T>
ClientRequest ServerTestBase::makeEncryptedRequest(const HttpMethod method, const std::string_view endpoint, const T& jwt1,
        std::optional<std::string_view> xAccessCode,
        std::optional<std::pair<std::string_view, std::string_view>> bodyContentType,
        Header::keyValueMap_t&& headerFields,
        std::optional<std::function<void(std::string&)>> requestManipulator)
{
    if (headerFields.find(Header::Authorization) == headerFields.end())
    {
        headerFields.insert(getAuthorizationHeaderForJwt(jwt1));
    }

    // Create the inner request
    ClientRequest request(Header(method, static_cast<std::string>(endpoint), Header::Version_1_1,
                                 std::move(headerFields), HttpStatus::Unknown),
                          "");

    if (xAccessCode.has_value())
    {
        request.setHeader(Header::XAccessCode, std::string(*xAccessCode));
    }
    if (bodyContentType.has_value())
    {
        request.setBody(std::string(bodyContentType->first));
        request.setHeader(Header::ContentType, std::string(bodyContentType->second));
    }

    // Encrypt with TEE protocol.

    auto teeRequest = mTeeProtocol.createRequest(MockCryptography::getEciesPublicKeyCertificate(), request, jwt1);
    if (requestManipulator.has_value())
        (*requestManipulator)(teeRequest);

    ClientRequest encryptedRequest(Header(HttpMethod::POST, "/VAU/0", Header::Version_1_1, {}, HttpStatus::Unknown),
                                   teeRequest);
    encryptedRequest.setHeader(Header::ContentType, "application/octet-stream");
    return encryptedRequest;
}

template ClientRequest ServerTestBase::makeEncryptedRequest<JWT>(const HttpMethod method, const std::string_view endpoint, const JWT& jwt1,
        std::optional<std::string_view> xAccessCode,
        std::optional<std::pair<std::string_view, std::string_view>> bodyContentType,
        Header::keyValueMap_t&& headerFields,
        std::optional<std::function<void(std::string&)>> requestManipulator);

template ClientRequest ServerTestBase::makeEncryptedRequest<std::string>(const HttpMethod method, const std::string_view endpoint, const std::string& jwt1,
        std::optional<std::string_view> xAccessCode,
        std::optional<std::pair<std::string_view, std::string_view>> bodyContentType,
        Header::keyValueMap_t&& headerFields,
        std::optional<std::function<void(std::string&)>> requestManipulator);

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


ClientResponse ServerTestBase::verifyResponse (const ClientResponse& outerResponse)
{
    EXPECT_EQ(outerResponse.getHeader().status(), HttpStatus::OK)
                    << "header status was " << toString(outerResponse.getHeader().status());
    EXPECT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
    EXPECT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), MimeType::binary);

    auto innerResponse = mTeeProtocol.parseResponse(outerResponse);
    validateInnerResponse(innerResponse);
    return innerResponse;
}

void ServerTestBase::validateInnerResponse(const ClientResponse& innerResponse) const
{
    const gsl::not_null repoView = Fhir::instance().structureRepository(model::Timestamp::now()).latest();
    std::optional<model::UnspecifiedResource> resourceForValidation;
    if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirXmlUtf8})
    {
        ASSERT_NO_FATAL_FAILURE((void)testutils::createResourceFromXml(innerResponse.getBody()));
    }
    else if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirJsonUtf8})
    {
        ASSERT_NO_FATAL_FAILURE((void)testutils::createResourceFromJson(innerResponse.getBody()));
    }
}


//NOLINTNEXTLINE(readability-function-cognitive-complexity)
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

Database::Factory ServerTestBase::createDatabaseFactory(ConnectionFactory connFactory)
{
    if (mHasPostgresSupport)
    {
        return [connFactory](HsmPool& hsmPool, KeyDerivation& keyDerivation){
            return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(connFactory()), hsmPool, keyDerivation); };
    }
    else
    {
        return [this](HsmPool& hsmPool, KeyDerivation& keyDerivation){
                return std::make_unique<DatabaseFrontend>(
                    std::make_unique<MockDatabaseProxy>(mMockDatabase), hsmPool, keyDerivation);
            };
    }
}

std::unique_ptr<Database> ServerTestBase::createDatabase()
{
    return createDatabaseFactory(&PostgresBackend::mainConnection)(mContext->getHsmPool(), mContext->getKeyDerivation());
}


pqxx::connection& ServerTestBase::getConnection()
{
    Expect(mHasPostgresSupport, "can not return Postgres connection because postgres support is disabled");
    if ( ! mConnection)
    {
        mConnection = std::make_unique<pqxx::connection>(PostgresConnection::defaultConnectString());
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
            "idNummer": "X020406082",
            "iss": "https://idp1.telematik.de/jwt",
            "jti": "<IDP>_01234567890123456789",
            "nonce": "fuu bar baz",
            "organizationName": "Organisation",
            "professionOID": "##PROFESSION-OID##",
            "sub": "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"
        })";

    jwtClaims = String::replaceAll(jwtClaims, "##PROFESSION-OID##", std::string(professionOID));
    // Token will expire from execution time T + 5 minutes.
    jwtClaims = String::replaceAll(jwtClaims, "##EXP##", std::to_string( +60l * 5 + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
    // Token is isseud  at execution time T - 1 minutes.
    jwtClaims = String::replaceAll(jwtClaims, "##IAT##", std::to_string( -60 + std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() )) );
    document.Parse(jwtClaims);
    return mJwtBuilder.getJWT(document);
}


std::vector<model::Task> ServerTestBase::addTasksToDatabase(const std::vector<TaskDescriptor>& descriptors)
{
    std::vector<model::Task> tasks;
    tasks.reserve(descriptors.size());
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

    Task task = createTask(descriptor.accessCode, descriptor.prescriptionType);
    const bool isPkv = descriptor.isPkv.value_or(model::canBePkv(descriptor.prescriptionType));
    task.setIsPkv(isPkv);

    if (descriptor.status == Task::Status::ready)
    {
        activateTask(task, descriptor.kvnrPatient, descriptor.expiryDate, {}, isPkv);
    }
    else if (descriptor.status == Task::Status::inprogress)
    {
        activateTask(task, descriptor.kvnrPatient, {}, {}, isPkv);
        acceptTask(task);
    }
    else if (descriptor.status == Task::Status::completed)
    {
        activateTask(task, descriptor.kvnrPatient, {}, {}, isPkv);
        acceptTask(task);
        closeTask(task, descriptor.telematicIdPharmacy.has_value() ? descriptor.telematicIdPharmacy.value() : mPharmacy.id());
    }
    return task;
}

Task ServerTestBase::createTask(
    const std::string& accessCode,
    model::PrescriptionType prescriptionType)
{
    Task task(prescriptionType, accessCode);
    auto database = createDatabase();
    PrescriptionId prescriptionId = database->storeTask(task);
    task.setPrescriptionId(prescriptionId);
    database->commitTransaction();
    return task;
}

void ServerTestBase::activateTask(Task& task, const std::string& kvnrPatient,
                                  std::optional<model::Timestamp> expiryDate,
                                  std::optional<ResourceTemplates::KbvBundleMvoOptions> mvoOptions,
                                  bool)
{
    const PrescriptionId prescriptionId = task.prescriptionId();
    const Kvnr kvnr{kvnrPatient};
    task.setKvnr(kvnr);

    const auto prescriptionBundleXmlString = mvoOptions.has_value()
        ? ResourceTemplates::kbvBundleMvoXml(*mvoOptions)
        : ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId, .kvnr = kvnrPatient});
    static const fhirtools::ValidatorOptions validatorOptions{.allowNonLiteralAuthorReference = true};
    const auto prescriptionBundle =
        ResourceFactory<KbvBundle>::fromXml(prescriptionBundleXmlString, *StaticData::getXmlValidator(),
                                            {.validatorOptions = validatorOptions})
            .getValidated(ProfileType::KBV_PR_ERP_Bundle);
    const std::vector<Patient> patients = prescriptionBundle.getResourcesByType<Patient>("Patient");

    ASSERT_FALSE(patients.empty());

    for (const auto& patient : patients)
    {
        ASSERT_EQ(patient.kvnr(), kvnrPatient);
    }

    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setExpiryDate(expiryDate.value_or(Timestamp::now() + std::chrono::hours(24) * 92));
    task.setAcceptDate(Timestamp::now());
    task.setStatus(Task::Status::ready);
    task.updateLastUpdate();

    auto privKey = MockCryptography::getIdpPrivateKey();
    auto cert = MockCryptography::getIdpPublicKeyCertificate();
    CadesBesSignature cadesBesSignature{ cert, privKey, prescriptionBundleXmlString };
    std::string encodedPrescriptionBundleXmlString = cadesBesSignature.getBase64();
    const Binary healthCareProviderPrescriptionBinary(*task.healthCarePrescriptionUuid(), encodedPrescriptionBundleXmlString);

    auto database = createDatabase();
    database->activateTask(task, healthCareProviderPrescriptionBinary, mJwtBuilder.makeJwtArzt());
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

std::vector<MedicationDispense> ServerTestBase::closeTask(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const std::optional<Timestamp>& medicationWhenPrepared,
    size_t numMedications)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    const Timestamp inProgessDate = task.lastModifiedDate();
    const Timestamp completedTimestamp = model::Timestamp::now();
    const std::string linkBase = "https://127.0.0.1:8080";
    const std::string authorIdentifier = linkBase + "/Device";
    const std::string prescriptionDigestIdentifier = "Binary/TestDigest";
    Composition compositionResource(telematicIdPharmacy, inProgessDate, completedTimestamp, authorIdentifier,
                                    prescriptionDigestIdentifier);
    Device deviceResource;
    const ::model::Binary prescriptionDigestResource{"TestDigest", "Test", ::model::Binary::Type::Digest};

    task.setReceiptUuid();
    task.setStatus(Task::Status::completed);
    task.updateLastUpdate();
    task.updateLastMedicationDispense();
    task.setOwner(telematicIdPharmacy);


    std::vector<model::MedicationDispense> medicationDispenses;
    std::vector<model::GemErpPrMedication> medications;
    for (size_t i = 0; i < numMedications; ++i)
    {
        if (medicationWhenPrepared.has_value())
        {
            auto [dispense, medication] =
                createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, *medicationWhenPrepared);
            medications.emplace_back(std::move(medication));
            medicationDispenses.emplace_back(std::move(dispense));
        }
        else
        {
            auto [dispense, medication] =
                createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, std::monostate{});
            medications.emplace_back(std::move(medication));
            medicationDispenses.emplace_back(std::move(dispense));
        }
        medicationDispenses.back().setId(model::MedicationDispenseId(medicationDispenses.back().prescriptionId(), i));
    }

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/", prescriptionId,
        compositionResource, authorIdentifier, deviceResource, "TestDigest", prescriptionDigestResource);

    auto database = createDatabase();
    database->updateTaskMedicationDispenseReceipt(task,
                                                  model::MedicationDispenseBundle{"", medicationDispenses, medications},
                                                  responseReceipt, mJwtBuilder.makeJwtApotheke());
    database->deleteCommunicationsForTask(task.prescriptionId());
    database->commitTransaction();

    return medicationDispenses;
}

void ServerTestBase::abortTask(Task& task)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    task.setStatus(model::Task::Status::cancelled);
    task.updateLastUpdate();

    auto database = createDatabase();
    database->deleteCommunicationsForTask(prescriptionId);
    database->updateTaskClearPersonalData(task);
    database->commitTransaction();
}

std::pair<model::MedicationDispense, model::GemErpPrMedication> ServerTestBase::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::variant<std::monostate, Timestamp, std::string>& whenPrepared) const
{
    PrescriptionId prescriptionId = task.prescriptionId();
    const auto kvnrPatient = task.kvnr().value();
    const auto& version = ResourceTemplates::Versions::GEM_ERP_current(whenHandedOver);
    Uuid medicationId{};

    const auto medicationDispenseString = ResourceTemplates::medicationDispenseXml({
        .id = model::MedicationDispenseId{prescriptionId, 0},
        .prescriptionId = prescriptionId,
        .kvnr = kvnrPatient.id(),
        .telematikId = telematicIdPharmacy,
        .whenHandedOver = whenHandedOver,
        .whenPrepared = whenPrepared,
        .gematikVersion = version,
        .medication = medicationId.toUrn(),
    });
    const auto medicationString = ResourceTemplates::medicationXml({
        .version = version,
        .id = medicationId.toString(),
    });

    return {MedicationDispense::fromXmlNoValidation(medicationDispenseString),
            model::GemErpPrMedication::fromXmlNoValidation(medicationString)};
}

std::vector<Uuid> ServerTestBase::addCommunicationsToDatabase(const std::vector<CommunicationDescriptor>& descriptors)
{
    std::vector<Uuid> communicationIds;
    communicationIds.reserve(descriptors.size());
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
    model::Communication communication = Communication::fromJsonNoValidation(builder.createJsonString());
    auto database = createDatabase();
    const auto communicationId = database->insertCommunication(communication);
    if (descriptor.received.has_value())
    {
        database->markCommunicationsAsRetrieved({*communicationId}, *descriptor.received, descriptor.recipient.name);
    }
    database->commitTransaction();
    communication.setId(communicationId.value());
    return communication;

}

model::ChargeItem ServerTestBase::addChargeItemToDatabase(const ChargeItemDescriptor& descriptor)
{
    auto& resourceManager = ResourceManager::instance();
    const auto chargeItemInput = resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input_marked.json");
    auto chargeItem = model::ChargeItem::fromJsonNoValidation(chargeItemInput);
    const auto dispenseItemXML = ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {{}}});
    const auto dispenseItemBundle = ::model::Bundle::fromXmlNoValidation(dispenseItemXML);

    const auto kbvBundlePkvXml =
        ResourceTemplates::kbvBundleXml({.prescriptionId = descriptor.prescriptionId, .kvnr = descriptor.kvnrStr});
    const auto& kbvBundlePkv = CryptoHelper::toCadesBesSignature(kbvBundlePkvXml);
    auto healthCarePrescriptionBundlePkv = model::Binary(descriptor.healthCarePrescriptionUuid, kbvBundlePkv);

    chargeItem.setPrescriptionId(descriptor.prescriptionId);
    chargeItem.setSubjectKvnr(descriptor.kvnrStr);
    chargeItem.setEntererTelematikId(descriptor.telematikId);
    chargeItem.setAccessCode(descriptor.accessCode);

    auto result = model::ChargeItem::fromJsonNoValidation(chargeItem.serializeToJsonString());

    auto database = createDatabase();
    database->storeChargeInformation(
        model::ChargeInformation{
            std::move(chargeItem), std::move(healthCarePrescriptionBundlePkv),
            model::Bundle::fromXmlNoValidation(kbvBundlePkvXml),
            model::Binary{dispenseItemBundle.getIdentifier().toString(),
                            CryptoHelper::toCadesBesSignature(dispenseItemBundle.serializeToJsonString())},
            model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML),
            model::Bundle::fromJsonNoValidation(
                String::replaceAll(
                    resourceManager.getStringResource("test/EndpointHandlerTest/receipt_template.json"),
                    "##PRESCRIPTION_ID##", descriptor.prescriptionId.toString()))});
    database->commitTransaction();

    return result;
}

void ServerTestBase::expectOperationOutcome(const ClientResponse& response, const std::string_view expectedDetailsText)
{
    EXPECT_TRUE(response.getHeader().contentType().has_value());
    std::optional<model::OperationOutcome> oo;
    if (response.getHeader().contentType()->starts_with("application/fhir+xml"))
    {
        ASSERT_NO_THROW(
            oo.emplace(model::OperationOutcome::fromXml(response.getBody(), *StaticData::getXmlValidator())));
    }
    else if (response.getHeader().contentType()->starts_with("application/fhir+json"))
    {
        ASSERT_NO_THROW(
            oo.emplace(model::OperationOutcome::fromJson(response.getBody(), *StaticData::getJsonValidator())));
    }
    ASSERT_TRUE(oo.has_value());
    EXPECT_FALSE(oo->issues().empty());
    EXPECT_EQ(oo->issues()[0].detailsText, expectedDetailsText);
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
