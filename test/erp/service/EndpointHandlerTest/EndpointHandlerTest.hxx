/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */
#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTEST_HXX

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/service/AuditEventHandler.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/StaticData.hxx"
#include "test_config.h"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>
#include <regex>
#include <variant>

template <typename TestClass>
class EndpointHandlerTestT : public TestClass
{
public:
    EndpointHandlerTestT()
        : dataPath(std::string{TEST_DATA_DIR} + "/EndpointHandlerTest")
        , mServiceContext(StaticData::makePcServiceContext([](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
            auto md = std::make_unique<MockDatabase>(hsmPool);
            md->fillWithStaticData();
            return std::make_unique<DatabaseFrontend>(std::move(md), hsmPool, keyDerivation);
        }))
    {
        mJwtBuilder = std::make_unique<JwtBuilder>(MockCryptography::getIdpPrivateKey());
    }

    model::Task addTaskToDatabase(
        SessionContext& sessionContext,
        model::Task::Status taskStatus,
        const std::string& accessCode,
        const std::string& insurant)
    {
        auto* database = sessionContext.database();
        model::Task task{ model::PrescriptionType::apothekenpflichigeArzneimittel, accessCode };
        task.setAcceptDate(model::Timestamp{ .0 });
        task.setExpiryDate(model::Timestamp{ .0 });
        task.setKvnr(model::Kvnr{insurant, model::Kvnr::Type::gkv});
        task.updateLastUpdate(model::Timestamp{ .0 });
        task.setStatus(taskStatus);
        model::PrescriptionId prescriptionId = database->storeTask(task);
        task.setPrescriptionId(prescriptionId);
        if (taskStatus != model::Task::Status::draft)
        {
            task.setHealthCarePrescriptionUuid();
            const std::optional<std::string_view> healthCarePrescriptionUuid =
                task.healthCarePrescriptionUuid().value();
            const auto& kbvBundle = ResourceTemplates::kbvBundleXml();
            const model::Binary healthCarePrescriptionBundle{
                healthCarePrescriptionUuid.value(), CryptoHelper::toCadesBesSignature(kbvBundle)};
            database->activateTask(task, healthCarePrescriptionBundle);
        }
        database->commitTransaction();
        return task;
    }

    void checkGetAllAuditEvents(const std::string& kvnr, const std::string& expectedResultFilename)//NOLINT(readability-function-cognitive-complexity)
    {
        GetAllAuditEventsHandler handler({});

        Header requestHeader{ HttpMethod::GET, "/AuditEvent/", 0, { {Header::AcceptLanguage, "de"} }, HttpStatus::Unknown};

        auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        ServerRequest serverRequest{ std::move(requestHeader) };
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
        ASSERT_NO_THROW((void)model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *StaticData::getXmlValidator(),
                                                         *StaticData::getInCodeValidator(), SchemaType::Gem_erxAuditEvent));

        auto expectedAuditEvent = model::AuditEvent::fromJsonNoValidation(
            FileHelper::readFileAsString(dataPath + "/" + expectedResultFilename));

        ASSERT_EQ(canonicalJson(auditEvent.serializeToJsonString()),
                  canonicalJson(expectedAuditEvent.serializeToJsonString()));
    }

    std::string replacePrescriptionId(const std::string& templateStr, const std::string& prescriptionId)
    {
        return String::replaceAll(templateStr, "##PRESCRIPTION_ID##", prescriptionId);
    }
    std::string replaceKvnr(const std::string& templateStr, const std::string& kvnr)
    {
        return String::replaceAll(templateStr, "##KVNR##", kvnr);
    }

protected:
    std::string dataPath;
    PcServiceContext mServiceContext;
    std::unique_ptr<JwtBuilder> mJwtBuilder;
};

using EndpointHandlerTest = EndpointHandlerTestT<::testing::Test>;

#endif//ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTEST_HXX
