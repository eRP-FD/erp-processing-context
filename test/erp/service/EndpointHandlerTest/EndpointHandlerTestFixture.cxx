#include "EndpointHandlerTestFixture.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/test_config.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <date/tz.h>

class HsmPool;
class KeyDerivation;

EndpointHandlerTest::EndpointHandlerTest()
    : dataPath(std::string{TEST_DATA_DIR} + "/EndpointHandlerTest")
    , mServiceContext(StaticData::makePcServiceContext([this](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
        return createDatabase(hsmPool, keyDerivation);
    }))
    , mJwtBuilder{std::make_unique<JwtBuilder>(MockCryptography::getIdpPrivateKey())}
{
}
EndpointHandlerTest::EndpointHandlerTest::~EndpointHandlerTest() = default;

std::unique_ptr<DatabaseFrontend> EndpointHandlerTest::createDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation)
{
    if (! mockDatabase)
    {
        mockDatabase = std::make_shared<MockDatabase>(hsmPool);
        mockDatabase->fillWithStaticData();
    }
    auto md = std::make_unique<MockDatabaseProxy>(mockDatabase);
    return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
}

void EndpointHandlerTest::checkGetAllAuditEvents(const std::string& kvnr, const std::string& expectedResultFilename,
                                                 size_t expectedCount)
{
    GetAllAuditEventsHandler handler({});

    Header requestHeader{HttpMethod::GET, "/AuditEvent/", 0, {{Header::AcceptLanguage, "de"}}, HttpStatus::Unknown};

    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(std::move(jwt));

    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    const auto auditEventBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(auditEventBundle.jsonDocument()),
        SchemaType::fhir));
    EXPECT_NO_FATAL_FAILURE(testutils::validate(auditEventBundle));

    A_24443_01.test("paging without prev");
    EXPECT_TRUE(auditEventBundle.getLink(model::Link::Self).has_value());
    EXPECT_TRUE(auditEventBundle.getLink(model::Link::First).has_value());
    EXPECT_FALSE(auditEventBundle.getLink(model::Link::Prev).has_value());
    if (expectedCount > 50)
    {
        EXPECT_TRUE(auditEventBundle.getLink(model::Link::Next).has_value());
    }

    auto auditEvents = auditEventBundle.getResourcesByType<model::AuditEvent>("AuditEvent");
    ASSERT_GE(auditEvents.size(), 1);

    auto& auditEvent = auditEvents.front();
    ASSERT_NO_THROW(
        (void) model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator()));

    auto expectedAuditEvent =
        model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/" + expectedResultFilename));

    ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
              canonicalJson(expectedAuditEvent.serializeToJsonString()));
}

void EndpointHandlerTest::insertTask(model::PrescriptionType prescriptionType, ResourceTemplates::TaskType taskType,
                                     const int64_t databaseId, const model::Timestamp& expirydate,
                                     const model::Kvnr& kvnr)
{
    const auto taskId = model::PrescriptionId::fromDatabaseId(prescriptionType, databaseId);
    auto validUntil = date::make_zoned(model::Timestamp::GermanTimezone, expirydate.toChronoTimePoint() + 24h);
    validUntil = floor<date::days>(validUntil.get_local_time());
    ResourceTemplates::TaskOptions taskOpts{
        .taskType = taskType,
        .prescriptionId = taskId,
        .expirydate = model::Timestamp(validUntil.get_sys_time()),
        .kvnr = kvnr.id(),
    };
    auto doc = model::NumberAsStringParserDocument::fromJson(ResourceTemplates::taskJson(taskOpts));
    auto task = model::Task::fromJson(doc);
    task.setIsPkv(! model::canBeGkv(prescriptionType));
    mockDatabase->insertTask(task);
    TLOG(INFO) << "inserted task with expiry date " << taskOpts.expirydate;
}

model::Task EndpointHandlerTest::addTaskToDatabase(SessionContext& sessionContext, model::Task::Status taskStatus,
                                                   const std::string& accessCode, const std::string& insurant,
                                                   const model::Timestamp lastUpdate)
{
    auto database = sessionContext.serviceContext.databaseFactory();
    model::Task task{model::PrescriptionType::apothekenpflichigeArzneimittel, accessCode};
    task.setAcceptDate(model::Timestamp{.0});
    task.setExpiryDate(model::Timestamp{.0});
    task.setKvnr(model::Kvnr{insurant});
    task.updateLastUpdate(lastUpdate);
    task.setStatus(taskStatus);
    task.setIsPkv(false);
    model::PrescriptionId prescriptionId = database->storeTask(task);
    task.setPrescriptionId(prescriptionId);
    if (taskStatus != model::Task::Status::draft)
    {
        task.setHealthCarePrescriptionUuid();
        const std::optional<std::string_view> healthCarePrescriptionUuid = task.healthCarePrescriptionUuid();
        const auto& kbvBundle = ResourceTemplates::kbvBundleXml();
        const model::Binary healthCarePrescriptionBundle{healthCarePrescriptionUuid.value(),
                                                         CryptoHelper::toCadesBesSignature(kbvBundle)};
        database->activateTask(task, healthCarePrescriptionBundle, mJwtBuilder->makeJwtArzt());
    }
    database->commitTransaction();
    return task;
}

std::optional<model::UnspecifiedResource>
EndpointHandlerTest::getMedicationDispenses(const std::string& kvnr, const model::PrescriptionId& prescriptionId)
{
    std::string identifier{model::resource::naming_system::prescriptionID};
    identifier += "|" + prescriptionId.toString();
    ServerRequest serverRequest{{HttpMethod::GET,
                                 "/MedicationDispense?identifier=" + identifier,
                                 Header::Version_1_1,
                                 {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                                 HttpStatus::Unknown}};
    serverRequest.setQueryParameters({{"identifier", identifier}});
    serverRequest.setAccessToken(JwtBuilder::testBuilder().makeJwtVersicherter(kvnr));
    AccessLog accessLog;
    std::optional<model::UnspecifiedResource> result;
    [&] {
        ServerResponse serverResponse;
        SessionContext session(mServiceContext, serverRequest, serverResponse, accessLog);
        GetAllMedicationDispenseHandler handler{{}};
        ASSERT_NO_THROW(handler.preHandleRequestHook(session));
        ASSERT_NO_THROW(handler.handleRequest(session));
        ASSERT_NO_THROW(result.emplace(model::UnspecifiedResource::fromJsonNoValidation(serverResponse.getBody())));
        ASSERT_NO_FATAL_FAILURE(testutils::validate(*result));
    }();
    return result;
}
