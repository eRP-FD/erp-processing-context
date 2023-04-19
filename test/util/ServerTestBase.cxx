/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ServerTestBase.hxx"
#include "TestUtils.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/mock/RegistrationMock.hxx"
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
    factories.databaseFactory = createDatabaseFactory();
    factories.redisClientFactory = []{return createRedisInstance();};
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
        deleteTxn.exec("DELETE FROM erp.task");
        deleteTxn.exec("DELETE FROM erp.task_169");
        deleteTxn.exec("DELETE FROM erp.task_200");
        deleteTxn.exec("DELETE FROM erp.task_209");
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


HttpsClient ServerTestBase::createClient (void)
{
    const auto& config = Configuration::instance();
    return HttpsClient("127.0.0.1", gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ADMIN_SERVER_PORT)),
                       30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
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


ClientResponse ServerTestBase::verifyOuterResponse (const ClientResponse& outerResponse)
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
    auto fhirBundleVersion = model::ResourceVersion::current<model::ResourceVersion::FhirProfileBundleVersion>();
    // Enable the output validation for the "after transition phase" case, because otherwise old profiles may occur.
    // Old profiles cannot be validated generically due to inconsistencies.
    if (! model::ResourceVersion::supportedBundles().contains(
            model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01))
    {
        std::optional<model::UnspecifiedResource> resourceForValidation;
        if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirXmlUtf8})
        {
            resourceForValidation = model::UnspecifiedResource::fromXmlNoValidation(innerResponse.getBody());
        }
        else if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirJsonUtf8})
        {
            resourceForValidation = model::UnspecifiedResource::fromJsonNoValidation(innerResponse.getBody());
        }
        if (resourceForValidation)
        {
            fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true};
            auto validationResult = resourceForValidation->genericValidate(fhirBundleVersion, options);
            auto filteredValidationErrors = testutils::validationResultFilter(validationResult, options);
            for (const auto& item : filteredValidationErrors)
            {
                ASSERT_LT(item.severity(), fhirtools::Severity::error) << to_string(item) << "\n"
                                                                       << innerResponse.getBody();
            }
        }
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
    return createDatabaseFactory()(mContext->getHsmPool(), mContext->getKeyDerivation());
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
            "idNummer": "X020406089",
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

void ServerTestBase::activateTask(Task& task, const std::string& kvnrPatient)
{
    PrescriptionId prescriptionId = task.prescriptionId();
    Kvnr kvnr{kvnrPatient, prescriptionId.isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv};
    task.setKvnr(kvnr);

    const auto prescriptionBundleXmlString = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .kvnr = kvnrPatient});
    static const fhirtools::ValidatorOptions validatorOptions{.allowNonLiteralAuthorReference = true};
    const auto prescriptionBundle =
            ResourceFactory<KbvBundle>::fromXml(prescriptionBundleXmlString, *StaticData::getXmlValidator(),
                                                {.validatorOptions = validatorOptions})
            .getValidated(SchemaType::KBV_PR_ERP_Bundle, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(),
                 model::ResourceVersion::supportedBundles());
    const std::vector<Patient> patients = prescriptionBundle.getResourcesByType<Patient>("Patient");

    ASSERT_FALSE(patients.empty());

    for (const auto& patient : patients)
    {
        ASSERT_EQ(patient.kvnr(), kvnrPatient);
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
    std::string encodedPrescriptionBundleXmlString = cadesBesSignature.getBase64();
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


    std::vector<model::MedicationDispense> medicationDispenses;
    for (size_t i = 0; i < numMedications; ++i)
    {
        medicationDispenses.emplace_back(
            createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, medicationWhenPrepared));
        medicationDispenses.back().setId(model::MedicationDispenseId(medicationDispenses.back().prescriptionId(), i));
    }

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/", prescriptionId,
        compositionResource, authorIdentifier, deviceResource, "TestDigest", prescriptionDigestResource);

    auto database = createDatabase();
    database->updateTaskMedicationDispenseReceipt(task, medicationDispenses, responseReceipt);
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

MedicationDispense ServerTestBase::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::optional<Timestamp>& whenPrepared) const
{
    const auto medicationDispenseString = ResourceTemplates::medicationDispenseXml({});

    MedicationDispense medicationDispense = MedicationDispense::fromXml(
            medicationDispenseString,
            *StaticData::getXmlValidator(),
            *StaticData::getInCodeValidator(),
            SchemaType::Gem_erxMedicationDispense);

    PrescriptionId prescriptionId = task.prescriptionId();
    const auto kvnrPatient = task.kvnr().value();

    medicationDispense.setPrescriptionId(prescriptionId);
    medicationDispense.setId({prescriptionId, 0});
    medicationDispense.setKvnr(kvnrPatient);
    medicationDispense.setTelematicId(model::TelematikId{telematicIdPharmacy});
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
    if (descriptor.messageType == model::Communication::MessageType::InfoReq)
    {
        builder.setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de");
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
    const auto dispenseItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
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
