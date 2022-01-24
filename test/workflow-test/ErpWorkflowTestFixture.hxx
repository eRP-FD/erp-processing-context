/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPWORKFLOWTESTFIXTURE_HXX
#define ERP_PROCESSING_CONTEXT_ERPWORKFLOWTESTFIXTURE_HXX

#include "test/mock/ClientTeeProtocol.hxx"
#include "test/util/JsonTestUtils.hxx"

#include "HttpsTestClient.hxx"
#include "TestClient.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/model/AuditEvent.hxx"
#include "erp/model/OperationOutcome.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/UrlHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/common/MimeType.hxx"
#include "test_config.h"
#include "test/util/StaticData.hxx"
#include "tools/jwt/JwtBuilder.hxx"

#include <gtest/gtest.h>
#include <rapidjson/error/en.h>
#include <regex>
#include <unordered_set>

class XmlValidator;
class JsonValidator;

// refer to http://hl7.org/fhir/R4/datatypes.html#instant
static const std::regex instantRegex{
    R"(([0-9]([0-9]([0-9][1-9]|[1-9]0)|[1-9]00)|[1-9]000)-(0[1-9]|1[0-2])-(0[1-9]|[1-2][0-9]|3[0-1])T([01][0-9]|2[0-3]):[0-5][0-9]:([0-5][0-9]|60)(\.[0-9]+)?(Z|(\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00)))"};// NOLINT

class ErpWorkflowTestBase
{
public:
    static constexpr std::string_view ProxyUserPseudonymHeader{"Userpseudonym"};
    static constexpr std::string_view VauPreUserPseudonymHeader{"PNP"};

    virtual ~ErpWorkflowTestBase();

    std::string toCadesBesSignature(const std::string& content,
                                    const std::optional<model::Timestamp>& signingTime = std::nullopt);

    void checkTaskMeta(const rapidjson::Value& meta);
    void checkTaskSingleIdentifier(const rapidjson::Value& id);

    void checkTaskIdentifiers(const rapidjson::Value& identifiers);

    void checkTask(const rapidjson::Value& task);

    void checkOperationOutcome(const std::string& responseBody,
                               bool isJson,
                               const model::OperationOutcome::Issue::Type expectedErrorCode,
                               const std::optional<std::string>& expectedIssueText = {},
                               const std::optional<std::string>& expectedIssueDiagnostics = {}) const;

    void getTaskFromBundle(std::optional<model::Task>& task,
        const model::Bundle& bundle);

    enum class UserPseudonymType
    {
        None = 0,
        PreUserPseudonym,
        UserPseudonym
    };



    class RequestArguments
    {
    public:
        RequestArguments(HttpMethod _method, std::string _vauPath, std::string _clearTextBody,
                         std::string _contentType = "", bool _skipOuterResponseVerification = true)
            : method(_method)
            , vauPath(std::move(_vauPath))
            , clearTextBody(std::move(_clearTextBody))
            , contentType(std::move(_contentType))
            , skipOuterResponseVerification(_skipOuterResponseVerification)
        {}
        RequestArguments&& withJwt(JWT _jwt) && { jwt = std::move(_jwt); return std::move(*this); }
        RequestArguments&& withHeader(const std::string& name, const std::string& value) &&
        {
            headerFields.emplace(name, value);
            return std::move(*this);
        }

        HttpMethod method;
        std::string vauPath;
        std::string clearTextBody;
        std::string contentType;
        bool skipOuterResponseVerification = false;
        std::optional<JWT> jwt;
        Header::keyValueMap_t headerFields = {{Header::UserAgent, std::string{"vau-cpp-it-test"}}};
        std::string overrideExpectedInnerOperation;
        std::string overrideExpectedInnerRole;

        RequestArguments(const RequestArguments&) = delete;
        RequestArguments(RequestArguments&&) = delete;
        RequestArguments& operator= (const RequestArguments&) = delete;
        RequestArguments& operator= (RequestArguments&&) = delete;
    };

    /// @returns {outerResponse, innerResponse}
    std::tuple<ClientResponse, ClientResponse> send(
        const RequestArguments& args,
        std::optional<std::function<void(std::string&)>> manipEncryptedInnerRequest = {}, // used to simulate errors;
        std::optional<std::function<void(Header&)>> manipInnerRequestHeader = {});        // used to simulate errors;

    std::optional<model::Task> taskCreate(model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel,
                                          HttpStatus expectedOuterStatus = HttpStatus::OK,
                                          HttpStatus expectedInnerStatus = HttpStatus::Created,
                                          const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    std::optional<model::Task> taskActivate(const model::PrescriptionId& prescriptionId,
        const std::string& accessCode,
        const std::string& qesBundle,
        HttpStatus expectedInnerStatus = HttpStatus::OK,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {},
        const std::optional<std::string>& expectedIssueText = {},
        const std::optional<std::string>& expectedIssueDiagnostics = {});

    std::optional<model::Bundle> taskAccept(const model::PrescriptionId& prescriptionId,
        const std::string& accessCode, HttpStatus expectedInnerStatus = HttpStatus::OK,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    std::optional<model::ErxReceipt> taskClose(const model::PrescriptionId& prescriptionId,
        const std::string& secret, const std::string& kvnr, HttpStatus expectedInnerStatus = HttpStatus::OK,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    void taskClose_MedicationDispense_invalidPrescriptionId(
        const model::PrescriptionId& prescriptionId,
        const std::string& invalidPrescriptionId,
        const std::string& secret,
        const std::string& kvnr);

    void taskClose_MedicationDispense_invalidWhenHandedOver(
        const model::PrescriptionId& prescriptionId,
        const std::string& secret,
        const std::string& kvnr,
        const std::string& invalidWhenHandedOver);

    void taskAbort(const model::PrescriptionId& prescriptionId,
        JWT jwt,
        const std::optional<std::string>& accessCode,
        const std::optional<std::string>& secret,
        const HttpStatus expectedStatus = HttpStatus::NoContent,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    void taskReject(const model::PrescriptionId& prescriptionId,
        const std::string& secret,
        HttpStatus expectedInnerStatus = HttpStatus::NoContent,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});
    void taskReject(const std::string& prescriptionIdString,
        const std::string& secret,
        HttpStatus expectedInnerStatus = HttpStatus::NoContent,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    std::optional<model::Bundle> taskGetId(const model::PrescriptionId& prescriptionId,
        const std::string& kvnrOrTid,
        const std::optional<std::string>& accessCodeOrSecret = std::nullopt,
        const HttpStatus expectedStatus = HttpStatus::OK,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {},
        bool withAuditEvents = false);

    std::optional<model::Bundle> taskGet(
        const std::string& kvnr,
        const std::string& searchArguments = "",
        const HttpStatus expectedStatus = HttpStatus::OK,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode = {});

    std::string makeQESBundle(const std::string& kvnr,
        const model::PrescriptionId& prescriptionId,
        const model::Timestamp& timestamp);

    std::optional<model::MedicationDispense> medicationDispenseGet(
        const std::string& kvnr,
        const model::PrescriptionId& prescriptionId);

    std::optional<model::Bundle> medicationDispenseGetAll(const std::string_view& searchArguments = {},
                                                          const std::optional<JWT>& jwt = std::nullopt);

    std::optional<model::Communication> communicationPost(
        const model::Communication::MessageType messageType,
        const model::Task& task,
        ActorRole senderRole, const std::string& sender,
        ActorRole recipientRole, const std::string& recipient,
        const std::string& content);

    std::optional<model::Bundle> communicationsGet(const JWT& jwt, const std::string_view& requestArgs = {});
    std::optional<model::Communication> communicationGet(const JWT& jwt, const Uuid& communicationId);
    void communicationDelete(const JWT& jwt, const Uuid& communicationId);
    void communicationDeleteAll(const JWT& jwt);

    std::size_t countTaskBasedCommunications(
        const model::Bundle& communicationsBundle,
        const model::PrescriptionId& prescriptionId);

    std::optional<model::Bundle> auditEventGet(
        const std::string& kvnr,
        const std::string& language,
        const std::string& searchArguments = "");

    std::optional<model::MetaData> metaDataGet(const ContentMimeType& acceptContentType);
    std::optional<model::Device> deviceGet(const ContentMimeType& acceptContentType);

    void checkTaskCreate(
        std::optional<model::PrescriptionId>& createdId,
        std::string& createdAccessCode,
        model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel);

    void checkTaskActivate(
        std::string& qesBundle,
        std::vector<model::Communication>& communications,
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr,
        const std::string& accessCode);

    void checkTaskAccept(
        std::string& createdSecret,
        std::optional<model::Timestamp>& lastModifiedDate,
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr,
        const std::string& accessCode,
        const std::string& qesBundle);

    void checkTaskClose(
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr,
        const std::string& secret,
        const model::Timestamp& lastModified,
        const std::vector<model::Communication>& communications);

    void checkTaskAbort(
        const model::PrescriptionId& prescriptionId,
        const JWT& jwt,
        const std::string& kvnr,
        const std::optional<std::string>& accessCode,
        const std::optional<std::string>& secret,
        const std::vector<model::Communication>& communications);

    void checkTaskReject(
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr,
        const std::string& accessCode,
        const std::string& secret);

    void checkCommunicationsDeleted(
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr,
        const std::vector<model::Communication>& communications);

    void checkAuditEvents(
        const model::PrescriptionId& prescriptionId,
        const std::string& insurantKvnr,
        const std::string& language,
        const model::Timestamp& startTime,
        const std::vector<std::string>& actorIdentifiers,
        const std::unordered_set<std::size_t>& actorTelematicIdIndices,
        const std::unordered_set<std::size_t>& noPrescriptionIdIndices,
        const std::vector<model::AuditEvent::SubType>& expectedActions);

    void checkAuditEventsFrom(const std::string& insurantKvnr);

    void checkAuditEventSorting(
            const std::string& kvnr,
            const std::string& searchCriteria,
            const std::function<void(const model::AuditEvent&, const model::AuditEvent&)>& compare);

    void checkAuditEventSearchSubType(const std::string& kvnr);

    void checkAuditEventPaging(
        const std::string& kvnr,
        const std::size_t numEventsExpected,
        const std::size_t pageSize,
        const std::string& addSearch = "");

    /// @note this already generates one audit event with the generated KVNR
    void generateNewRandomKVNR(std::string& kvnr);

    static std::shared_ptr<XmlValidator> getXmlValidator();
    static std::shared_ptr<JsonValidator> getJsonValidator();

    std::optional<Certificate> retrieveEciesRemoteCertificate();
    Certificate getEciesCertificate (void);

protected:
    virtual std::string medicationDispense(const std::string& kvnr,
                                           const std::string& prescriptionIdForMedicationDispense,
                                           const std::string& whenHandedOver);

private:
    void sendInternal(
        std::tuple<ClientResponse, ClientResponse>& result, const RequestArguments& args,
        std::optional<std::function<void(std::string&)>> manipEncryptedInnerRequest = {},
        std::optional<std::function<void(Header&)>> manipInnerRequestHeader = {});
    virtual std::string creatTeeRequest(const Certificate& serverPublicKeyCertificate, const ClientRequest& request,
                                        const JWT& jwt);
    virtual std::string creatUnvalidatedTeeRequest(const Certificate& serverPublicKeyCertificate, const ClientRequest& request,
                                                   const JWT& jwt);

    void taskCreateInternal(std::optional<model::Task>& task, HttpStatus expectedOuterStatus,
                            HttpStatus expectedInnerStatus,
                            const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                            model::PrescriptionType workflowType);

    static void makeQESBundleInternal (std::string& qesBundle,
        const std::string& kvnr,
        const model::PrescriptionId& prescriptionId,
        const model::Timestamp& now);

    void taskActivateInternal(std::optional<model::Task>& task,
        const model::PrescriptionId& prescriptionId,
        const std::string& accessCode,
        const std::string& qesBundle,
        HttpStatus expectedInnerStatus,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
        const std::optional<std::string>& expectedIssueText,
        const std::optional<std::string>& expectedIssueDiagnostics);

    void taskAcceptInternal(std::optional<model::Bundle>& bundle,
        const model::PrescriptionId& prescriptionId,
        const std::string& accessCode, HttpStatus expectedInnerStatus,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode);

    void taskCloseInternal(std::optional<model::ErxReceipt>& receipt,
        const model::PrescriptionId& prescriptionId,
        const std::string& secret,
        const std::string& kvnr,
        const std::string& prescriptionIdForMedicationDispense, // for test, normally equals prescriptionId;
        HttpStatus expectedInnerStatus,
        const std::optional<model::OperationOutcome::Issue::Type>& expectedErrorCode,
        const std::optional<std::string>& expectedErrorText,
        const std::optional<std::string>& expectedDiagnostics,
        const std::string& whenHandedOver);

    void taskGetIdInternal(std::optional<model::Bundle>& taskBundle,
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnrOrTid,
        const std::optional<std::string>& accessCodeOrSecret,
        const HttpStatus expectedStatus,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
        bool withAuditEvents = false);

    void taskGetInternal(
        std::optional<model::Bundle>& taskBundle,
        const std::string& kvnr,
        const std::string& searchArguments,
        const HttpStatus expectedStatus,
        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode);

    void medicationDispenseGetInternal(
        std::optional<model::MedicationDispense>& medicationDispense,
        const std::string& kvnr,
        const model::PrescriptionId& prescriptionId);

    void medicationDispenseGetAllInternal(
        std::optional<model::Bundle>& medicationDispenseBundle,
        const std::string_view& searchArguments,
        const std::optional<JWT>& jwt);

    void communicationPostInternal(
        std::optional<model::Communication>& communicationResponse,
        model::Communication::MessageType messageType,
        const model::Task& task,
        ActorRole senderRole, const std::string& sender,
        ActorRole recipientRole, const std::string& recipient,
        const std::string& content);

    void communicationsGetInternal(std::optional<model::Bundle>& communicationsBundle, const JWT& jwt,
                                   const std::string_view& requestArgs);
    void communicationGetInternal(std::optional<model::Communication>& communication, const JWT& jwt, const Uuid& communicationId);

    void auditEventGetInternal(
        std::optional<model::Bundle>& auditEventBundle,
        const std::string& kvnr,
        const std::string& language,
        const std::string& searchArguments);

    void metaDataGetInternal(
        std::optional<model::MetaData>& metaData,
        const ContentMimeType& acceptContentType);

    void deviceGetInternal(
        std::optional<model::Device>& device,
        const ContentMimeType& acceptContentType);

    void getMedicationDispenseForTask(
        std::optional<model::MedicationDispense>& medicationDispenseForTask,
        const model::PrescriptionId& prescriptionId,
        const std::string& kvnr);

    static std::string toExpectedOperation(const RequestArguments& args);

    std::string toExpectedRole(const RequestArguments& args) const;

public:

    ClientTeeProtocol teeProtocol;

    virtual JWT jwtVersicherter() const { return JwtBuilder::testBuilder().makeJwtVersicherter("X123456789"); }
    virtual JWT jwtArzt() const { return JwtBuilder::testBuilder().makeJwtArzt(); }
    virtual JWT jwtApotheke() const { return JwtBuilder::testBuilder().makeJwtApotheke(); }

    JWT defaultJwt() const { return jwtVersicherter(); }

    std::string getAuthorizationBearerValueForJwt(const JWT& jwt);

    std::string dataPath{std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest"};

    std::string userPseudonym{"0"};
    UserPseudonymType userPseudonymType{UserPseudonymType::None};
    std::unique_ptr<TestClient> client = TestClient::create(StaticData::getXmlValidator());

    void writeCurrentTestOutputFile(
        const std::string& testOutput,
        const std::string& fileExtension,
        const std::string& marker = std::string());
};

template <typename TestClass>
class ErpWorkflowTestTemplate : public TestClass, public ErpWorkflowTestBase {

};

using ErpWorkflowTest = ErpWorkflowTestTemplate<::testing::Test>;
using ErpWorkflowTestP = ErpWorkflowTestTemplate<::testing::TestWithParam<model::PrescriptionType>>;


#endif//ERP_PROCESSING_CONTEXT_ERPWORKFLOWTESTFIXTURE_HXX
