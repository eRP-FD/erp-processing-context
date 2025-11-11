// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/eu/GemErpEuPrTaskInput.hxx"
#include "erp/service/eu/PostGetPrescriptionsHandler.hxx"
#include "shared/model/KbvBundle.hxx"
#include "test/erp/service/EndpointHandlerTest/EuFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/StaticData.hxx"

using namespace ResourceTemplates;

class EuPostGetPrescriptionsHandlerTest : public EuFixture
{
public:
    void SetUp() override
    {
        EuFixture::SetUp();
        // mockDatabase = std::make_unique<MockDatabase>(mServiceContext.getHsmPool());
        // createDatabase(mServiceContext.getHsmPool(), mServiceContext.getKeyDerivation());
        allowCountry("FR");
        giveConsent(kvnr);
        grant(kvnr, accessCode);
        options.kvnr = kvnr;
        options.accessCode = accessCode;

        auto task1 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pidReady160, .kvnr = kvnr}));
        auto task2 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pidReady200, .kvnr = kvnr}));
        // DRAFT:
        auto task3 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pidDraft200, .kvnr = kvnr}));
        // IN_PROGRESS:
        auto task4 = model::Task::fromJsonNoValidation(
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::InProgress,
                                         .prescriptionId = pidInProgress160_Own,
                                         .kvnr = kvnr,
                                         .owningPharmacy = telematikId}));
        // IN_PROGRESS:
        auto task5 = model::Task::fromJsonNoValidation(
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::InProgress,
                                         .prescriptionId = pidInProgress160_Foreign,
                                         .kvnr = kvnr,
                                         .owningPharmacy = "3-SMC-B-Testkarte-XXXXXXXXXXXXXXXXXXX"}));
        // MVO, READY:
        auto task6 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pidReady160Mvo, .kvnr = kvnr}));

        // They are sorted by authoredOn by the get-eu-prescriptions handler;
        const auto kbvBundle1Xml =
            ResourceTemplates::kbvBundleXml({.prescriptionId = pidReady160,
                                             .authoredOn = model::Timestamp::now() - std::chrono::days{1},
                                             .kvnr = kvnr});
        const auto kbvBundle2Xml = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = pidReady200, .authoredOn = model::Timestamp::now(), .kvnr = kvnr});
        const auto kbvBundle4Xml =
            ResourceTemplates::kbvBundleXml({.prescriptionId = pidInProgress160_Own,
                                             .authoredOn = model::Timestamp::now() - std::chrono::days{2},
                                             .kvnr = kvnr});
        const auto kbvBundle5Xml =
            ResourceTemplates::kbvBundleXml({.prescriptionId = pidInProgress160_Foreign,
                                             .authoredOn = model::Timestamp::now() - std::chrono::days{2},
                                             .kvnr = kvnr});
        // MVO:
        const auto kbvBundle6Xml = ResourceTemplates::kbvBundleMvoXml(
            {.prescriptionId = pidReady160Mvo,
             .kvnr = kvnr,
             .authoredOn = model::Timestamp::now() - std::chrono::days{1},
             .redeemPeriodStart = (model::Timestamp::now() - std::chrono::days{1}).toGermanDate()});

        const auto& kbvBundle1 = CryptoHelper::toCadesBesSignature(kbvBundle1Xml);
        const auto& kbvBundle2 = CryptoHelper::toCadesBesSignature(kbvBundle2Xml);
        const auto& kbvBundle4 = CryptoHelper::toCadesBesSignature(kbvBundle4Xml);
        const auto& kbvBundle5 = CryptoHelper::toCadesBesSignature(kbvBundle5Xml);
        const auto& kbvBundle6 = CryptoHelper::toCadesBesSignature(kbvBundle6Xml);

        mockDatabase->insertTask(
            task1, std::nullopt,
            model::Binary{task1.healthCarePrescriptionUuid().value(), kbvBundle1}.serializeToJsonString());
        mockDatabase->insertTask(
            task2, std::nullopt,
            model::Binary{task2.healthCarePrescriptionUuid().value(), kbvBundle2}.serializeToJsonString());
        mockDatabase->insertTask(task3);
        mockDatabase->insertTask(
            task4, std::nullopt,
            model::Binary{task4.healthCarePrescriptionUuid().value(), kbvBundle4}.serializeToJsonString());
        mockDatabase->insertTask(
            task5, std::nullopt,
            model::Binary{task5.healthCarePrescriptionUuid().value(), kbvBundle5}.serializeToJsonString());
        mockDatabase->insertTask(
            task6, std::nullopt,
            model::Binary{task6.healthCarePrescriptionUuid().value(), kbvBundle6}.serializeToJsonString());
    }

    void markTask(const model::PrescriptionId& prescriptionId)
    {
        auto db = mServiceContext.databaseFactory();
        auto taskAndKey = db->retrieveTaskForUpdate(prescriptionId);
        auto patchParam = model::GemErpEuPrTaskInput::fromJsonNoValidation(
            R"({"resourceType": "Parameters", "id": "Example-PATCH-Task-Single-Input-Request-True", "meta": {"profile":  ["https://gematik.de/fhir/erp-eu/StructureDefinition/GEM_ERPEU_PR_PAR_PATCH_Task_Input|1.0"]}, "parameter":  [{"name": "eu-isRedeemableByPatientAuthorization", "valueBoolean": true}]})");
        taskAndKey->task.updateLastUpdate();
        db->setTaskEuRedeemableByPatient(taskAndKey->task, patchParam);
        db->commitTransaction();
    }

    void markAll()
    {
        markTask(pidReady160);
        markTask(pidReady200);
        markTask(pidDraft200);
        markTask(pidInProgress160_Own);
        markTask(pidInProgress160_Foreign);
        markTask(pidReady160Mvo);
    }

    // GEMREQ-start checkTaskStatus
    void checkTaskStatus(const model::PrescriptionId& prescriptionId, const model::Task::Status& status,
                         bool expectSecret = false, const std::optional<std::string>& expectOwner = std::nullopt)
    {
        auto db = mServiceContext.databaseFactory();
        auto taskAndKey = db->retrieveTaskForUpdate(prescriptionId);
        db->commitTransaction();
        ASSERT_TRUE(taskAndKey.has_value());
        ASSERT_EQ(taskAndKey->task.status(), status);
        EXPECT_EQ(taskAndKey->task.secret().has_value(), expectSecret);
        EXPECT_EQ(taskAndKey->task.owner(), expectOwner);
    }
    // GEMREQ-end checkTaskStatus

    std::string kvnr = "X199999999";
    std::string accessCode = "aaaaaa";
    std::string telematikId = "3-SMC-B-NCPEH-883110000120312";
    EuPostGetPrescriptionsOptions options;
    model::PrescriptionId pidReady160{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1)};
    model::PrescriptionId pidReady200{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 2)};
    model::PrescriptionId pidDraft200{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 3)};
    model::PrescriptionId pidInProgress160_Own{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4)};
    model::PrescriptionId pidInProgress160_Foreign{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 5)};
    model::PrescriptionId pidReady160Mvo{
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 6)};
    EnvironmentVariableGuard euFeatureToggleGuard{ConfigurationKey::FEATURE_EU, "true"};
};

TEST_F(EuPostGetPrescriptionsHandlerTest, demographicsNotFound)
{

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions?_count=1",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setQueryParameters({{{"_count", "1"}}});
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

    eu::PostGetPrescriptionsHandler handler{};
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                      "Keine einlösbaren Rezepte gefunden");
}

TEST_F(EuPostGetPrescriptionsHandlerTest, demographicsFilterStatus)
{
    A_27587.test("Filter Status - Abfrage der aktuellsten Verordnungsinformationen");
    markTask(pidDraft200);             // is DRAFT
    markTask(pidInProgress160_Own);    // is IN-PROGRESS
    markTask(pidInProgress160_Foreign);// is IN-PROGRESS
    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions?_count=1",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setQueryParameters({{{"_count", "1"}}});
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

    eu::PostGetPrescriptionsHandler handler{};
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                      "Keine einlösbaren Rezepte gefunden");
}

TEST_F(EuPostGetPrescriptionsHandlerTest, demographicsOK)
{
    markAll();

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::demographics;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setQueryParameters({{{"_count", "1"}}});
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;

    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirXmlUtf8});


        std::optional<model::Bundle> responseBundle;
        ASSERT_NO_THROW(responseBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()))
            << serverResponse.getBody();
        EXPECT_NO_FATAL_FAILURE(testutils::validate(value(responseBundle)));
        ASSERT_TRUE(responseBundle.has_value());
        EXPECT_EQ(responseBundle->getResourceCount(), 1);
        std::optional<model::KbvBundle> kbvBundle;
        ASSERT_NO_THROW(kbvBundle.emplace(responseBundle->getUniqueResourceByType<model::KbvBundle>()));
        ASSERT_TRUE(kbvBundle.has_value());
        //    ASSERT_NO_THROW(model::Bundle::fromJson(kbvBundle->serializeToJsonString(), *StaticData::getJsonValidator()));

        A_27064.test("absteigend sortiert nach dem medicationrequest.authored-on Datum");
        ASSERT_EQ(kbvBundle->getIdentifier().toString(), pidReady200.toString());
    }

    checkTaskStatus(pidReady160, model::Task::Status::ready);
    checkTaskStatus(pidReady200, model::Task::Status::ready);
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_list_NotFound)
{
    A_27065.test("Abfrage der aktuellsten Verordnungsinformationen");
    A_27066.test("Abfrage aller einlösbaren Verordnungsinformationen");
    A_27067.test("Abfrage nach Liste Rezept-Ids");

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    // The difference to the demographis use case is the missing _count=1 parameter
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                          "Keine einlösbaren Rezepte gefunden");
    }
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_list_FilterStatus)
{
    A_27588.test("Filter Status - Abfrage aller einlösbaren Verordnungsinformationen");
    markTask(pidDraft200);             // is DRAFT
    markTask(pidInProgress160_Own);    // is IN-PROGRESS
    markTask(pidInProgress160_Foreign);// is IN-PROGRESS

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    // The difference to the demographis use case is the missing _count=1 parameter
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                          "Keine einlösbaren Rezepte gefunden");
    }
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_list_Expired)
{
    A_27063_01.test("Task.ExpiryDate nicht vor dem aktuellen Datum");
    auto pId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 33);
    auto taskX = model::Task::fromJsonNoValidation(
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready,
                                     .prescriptionId = pId,
                                     .expirydate = model::Timestamp::now() - std::chrono::days{1},
                                     .kvnr = kvnr}));
    const auto kbvBundleXXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = pId, .authoredOn = model::Timestamp::now() - std::chrono::days{1}, .kvnr = kvnr});
    const auto& kbvBundleX = CryptoHelper::toCadesBesSignature(kbvBundleXXml);
    mockDatabase->insertTask(
        taskX, std::nullopt,
        model::Binary{taskX.healthCarePrescriptionUuid().value(), kbvBundleX}.serializeToJsonString());

    markTask(pId);

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    // The difference to the demographis use case is the missing _count=1 parameter
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                          "Keine einlösbaren Rezepte gefunden");
    }
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_list_MvoStartDateNotValid)
{
    A_27063_01.test("MVO Start Date not valid");
    auto pId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 33);
    auto taskX = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pId, .kvnr = kvnr}));
    const auto kbvBundleXXml = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = pId,
         .kvnr = kvnr,
         .authoredOn = model::Timestamp::now() - std::chrono::days{1},
         .redeemPeriodStart = (model::Timestamp::now() + std::chrono::days{1}).toGermanDate()});
    const auto& kbvBundleX = CryptoHelper::toCadesBesSignature(kbvBundleXXml);
    mockDatabase->insertTask(
        taskX, std::nullopt,
        model::Binary{taskX.healthCarePrescriptionUuid().value(), kbvBundleX}.serializeToJsonString());

    markTask(pId);

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    // The difference to the demographis use case is the missing _count=1 parameter
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(handler.handleRequest(sessionContext), HttpStatus::NotFound,
                                          "Keine einlösbaren Rezepte gefunden");
    }
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_list)
{
    A_27065.test("Abfrage der aktuellsten Verordnungsinformationen");
    A_27066.test("Abfrage aller einlösbaren Verordnungsinformationen");
    A_27067.test("Abfrage nach Liste Rezept-Ids");

    markAll();

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_list;
    options.prescriptionIds.clear();
    auto xml = euPostGetPrescriptionsXml(options);

    // The difference to the demographis use case is the missing _count=1 parameter
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirXmlUtf8});


        std::optional<model::Bundle> responseBundle;
        ASSERT_NO_THROW(responseBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
        ASSERT_TRUE(responseBundle.has_value());
        EXPECT_NO_FATAL_FAILURE(testutils::validate(*responseBundle));
        EXPECT_EQ(responseBundle->getResourceCount(), 3);
        std::vector<model::KbvBundle> kbvBundles;
        ASSERT_NO_THROW(kbvBundles = responseBundle->getResourcesByType<model::KbvBundle>());
        ASSERT_EQ(kbvBundles.size(), 3);
        std::vector<int64_t> haveIds;
        for (const auto& kbvBundle : kbvBundles)
        {
            haveIds.emplace_back(kbvBundle.getIdentifier().toDatabaseId());
        }

        A_27064.test("absteigend sortiert nach dem medicationrequest.authored-on Datum");
        EXPECT_EQ(haveIds[0], pidReady200.toDatabaseId());
        EXPECT_EQ(haveIds[1], pidReady160.toDatabaseId());
        EXPECT_EQ(haveIds[2], pidReady160Mvo.toDatabaseId());
    }

    checkTaskStatus(pidReady160, model::Task::Status::ready);
    checkTaskStatus(pidReady200, model::Task::Status::ready);
    checkTaskStatus(pidReady160Mvo, model::Task::Status::ready);
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_retrieval1)
{
    markAll();

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval;
    // GEMREQ-start A_27582#1
    options.prescriptionIds.clear();
    options.prescriptionIds.emplace_back(pidReady160.toString());
    // GEMREQ-end A_27582#1
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirXmlUtf8});


        std::optional<model::Bundle> responseBundle;
        ASSERT_NO_THROW(responseBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
        ASSERT_TRUE(responseBundle.has_value());
        EXPECT_NO_FATAL_FAILURE(testutils::validate(*responseBundle));
        EXPECT_EQ(responseBundle->getResourceCount(), 1);
        std::vector<model::KbvBundle> kbvBundles;
        ASSERT_NO_THROW(kbvBundles = responseBundle->getResourcesByType<model::KbvBundle>());
        ASSERT_EQ(kbvBundles.size(), 1);
        std::vector<int64_t> haveIds;
        for (const auto& kbvBundle : kbvBundles)
        {
            haveIds.emplace_back(kbvBundle.getIdentifier().toDatabaseId());
        }
        EXPECT_EQ(haveIds[0], pidReady160.toDatabaseId());
    }

    A_27580.test("Statuswechsel Task");
    A_27581.test("Secret");
    A_27582.test("Task Owner");
    // GEMREQ-start A_27582#2
    checkTaskStatus(pidReady160, model::Task::Status::inprogress, true, telematikId);
    // GEMREQ-end A_27582#2
    checkTaskStatus(pidReady200, model::Task::Status::ready);
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_retrieval2)
{
    markAll();

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval;
    // GEMREQ-start A_27582#3
    options.prescriptionIds.clear();
    options.prescriptionIds.emplace_back(pidReady160.toString());
    options.prescriptionIds.emplace_back(pidReady200.toString());
    options.prescriptionIds.emplace_back(pidReady160Mvo.toString());
    // GEMREQ-end A_27582#3
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirXmlUtf8});


        std::optional<model::Bundle> responseBundle;
        ASSERT_NO_THROW(responseBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
        ASSERT_TRUE(responseBundle.has_value());
        EXPECT_NO_FATAL_FAILURE(testutils::validate(*responseBundle));
        EXPECT_EQ(responseBundle->getResourceCount(), 3);
        std::vector<model::KbvBundle> kbvBundles;
        ASSERT_NO_THROW(kbvBundles = responseBundle->getResourcesByType<model::KbvBundle>());
        ASSERT_EQ(kbvBundles.size(), 3);
        std::vector<int64_t> haveIds;
        for (const auto& kbvBundle : kbvBundles)
        {
            haveIds.emplace_back(kbvBundle.getIdentifier().toDatabaseId());
        }
        A_27064.test("absteigend sortiert nach dem medicationrequest.authored-on Datum");
        EXPECT_EQ(haveIds[0], pidReady200.toDatabaseId());
        EXPECT_EQ(haveIds[1], pidReady160.toDatabaseId());
        EXPECT_EQ(haveIds[2], pidReady160Mvo.toDatabaseId());
    }

    A_27580.test("Statuswechsel Task");
    A_27581.test("Secret");
    A_27582.test("Task Owner");
    // GEMREQ-start A_27582#4
    checkTaskStatus(pidReady160, model::Task::Status::inprogress, true, telematikId);
    checkTaskStatus(pidReady200, model::Task::Status::inprogress, true, telematikId);
    checkTaskStatus(pidReady160Mvo, model::Task::Status::inprogress, true, telematikId);
    // GEMREQ-end A_27582#4
}

TEST_F(EuPostGetPrescriptionsHandlerTest, e_prescriptions_retrievalFilterStatus)
{
    A_27589.test("Filter Status - Abfrage nach Liste Rezept-Ids");
    markTask(pidDraft200);             // is DRAFT
    markTask(pidInProgress160_Own);    // is IN-PROGRESS (ours)
    markTask(pidInProgress160_Foreign);// is IN-PROGRESS (foreign)

    options.requestType = model::GemErpEuPrParGetPrescriptionInput::RequestType::e_prescriptions_retrieval;
    options.prescriptionIds.clear();
    options.prescriptionIds.emplace_back(pidInProgress160_Own.toString());
    auto xml = euPostGetPrescriptionsXml(options);

    ServerRequest request{Header{HttpMethod::POST,
                                 "/$get-eu-prescriptions",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(xml);
    request.setAccessToken(mJwtBuilder->makeJwtNcpeh());
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};

        eu::PostGetPrescriptionsHandler handler{};
        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
        EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirXmlUtf8});


        std::optional<model::Bundle> responseBundle;
        ASSERT_NO_THROW(responseBundle = model::Bundle::fromXmlNoValidation(serverResponse.getBody()));
        ASSERT_TRUE(responseBundle.has_value());
        EXPECT_NO_FATAL_FAILURE(testutils::validate(*responseBundle));
        EXPECT_EQ(responseBundle->getResourceCount(), 1);
        std::vector<model::KbvBundle> kbvBundles;
        ASSERT_NO_THROW(kbvBundles = responseBundle->getResourcesByType<model::KbvBundle>());
        ASSERT_EQ(kbvBundles.size(), 1);
        std::vector<int64_t> haveIds;
        for (const auto& kbvBundle : kbvBundles)
        {
            haveIds.emplace_back(kbvBundle.getIdentifier().toDatabaseId());
        }
        EXPECT_EQ(haveIds[0], pidInProgress160_Own.toDatabaseId());
    }

    checkTaskStatus(pidInProgress160_Own, model::Task::Status::inprogress, true, telematikId);
}
