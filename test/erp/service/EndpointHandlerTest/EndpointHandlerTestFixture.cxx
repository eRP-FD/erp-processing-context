#include "EndpointHandlerTestFixture.hxx"

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "erp/service/MedicationDispenseHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test/test_config.h"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"
#include "shared/util/FileHelper.hxx"

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
        mockDatabase = std::make_unique<MockDatabase>(hsmPool);
        mockDatabase->fillWithStaticData();
    }
    auto md = std::make_unique<MockDatabaseProxy>(*mockDatabase);
    return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
}

void EndpointHandlerTest::checkGetAllAuditEvents(const std::string& kvnr, const std::string& expectedResultFilename)
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

    auto auditEvents = auditEventBundle.getResourcesByType<model::AuditEvent>("AuditEvent");
    ASSERT_EQ(auditEvents.size(), 1);

    auto& auditEvent = auditEvents.front();
    ASSERT_NO_THROW(
        (void) model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator()));

    auto expectedAuditEvent =
        model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/" + expectedResultFilename));

    ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
              canonicalJson(expectedAuditEvent.serializeToJsonString()));
}

model::Task EndpointHandlerTest::addTaskToDatabase(SessionContext& sessionContext, model::Task::Status taskStatus,
                                                   const std::string& accessCode, const std::string& insurant,
                                                   const model::Timestamp lastUpdate)
{
    auto* database = sessionContext.database();
    model::Task task{model::PrescriptionType::apothekenpflichigeArzneimittel, accessCode};
    task.setAcceptDate(model::Timestamp{.0});
    task.setExpiryDate(model::Timestamp{.0});
    task.setKvnr(model::Kvnr{insurant, model::Kvnr::Type::gkv});
    task.updateLastUpdate(lastUpdate);
    task.setStatus(taskStatus);
    model::PrescriptionId prescriptionId = database->storeTask(task);
    task.setPrescriptionId(prescriptionId);
    if (taskStatus != model::Task::Status::draft)
    {
        task.setHealthCarePrescriptionUuid();
        const std::optional<std::string_view> healthCarePrescriptionUuid = task.healthCarePrescriptionUuid().value();
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
        ASSERT_NO_THROW(testutils::bestEffortValidate(*result));
    }();
    return result;
}
