/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASE_HXX


#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/JsonTestUtils.hxx"

#include "shared/network/client/HttpsClient.hxx"
#include "shared/crypto/Jwt.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Task.hxx"
#include "shared/model/TelematikId.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Uuid.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>
#include <pqxx/transaction>

std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const JWT& jwt);
std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const std::string& jwt);

class MockDatabase;
/**
 * You can use this base class for tests that require a running server.
 */
class ServerTestBase
    : public testing::Test
{
public:
    const std::string InfoReqMessage =
        "Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.";
    const std::string ReplyMessage =
        R"({"version":1,"supplyOptionsType":"onPremise","info_text":"Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten."})";
    const std::string DispReqMessage =
        R"({"version":1,"supplyOptionsType":"delivery","hint":"Bitte schicken Sie einen Boten."})";
    const std::string RepresentativeMessageByInsurant =
        "Hallo, kannst Du das Medikament fuer mich holen?";
    const std::string RepresentativeMessageByRepresentative =
        "Ja, mache ich gerne.";
    constexpr static size_t serverThreadCount = 2;

    explicit ServerTestBase (bool forceMockDatabase = false);
    ~ServerTestBase() override;

    void SetUp (void) override;
    void TearDown (void) override;

    HttpsClient createClient (void);

    // T can be JWT or std::string, auto-resolved.
    template <typename T>
    ClientRequest makeEncryptedRequest(
        const HttpMethod method, const std::string_view endpoint, const T& jwt1,
        std::optional<std::string_view> xAccessCode = {},
        std::optional<std::pair<std::string_view, std::string_view>> bodyContentType = {},
        Header::keyValueMap_t&& headerFields = {},
        std::optional<std::function<void(std::string&)>> requestManipulator = {}); // used to simulate faulty messages

    virtual Header createPostHeader (const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual Header createGetHeader (const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual Header createDeleteHeader(const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual ClientRequest encryptRequest (const ClientRequest& innerRequest, const std::optional<const JWT>& jwtToken = {});
    virtual ClientResponse verifyOuterResponse (const ClientResponse& outerResponse);
    void validateInnerResponse(const ClientResponse& innerResponse) const;
    virtual void verifyGenericInnerResponse (
        const ClientResponse& innerResponse,
        const HttpStatus expectedStatus = HttpStatus::OK,
        const std::string& expectedMimeType = ContentMimeType::fhirJsonUtf8);

    pqxx::connection& getConnection (void);
    pqxx::work createTransaction (void);

    JWT jwtWithProfessionOID (const std::string_view professionOID);

    struct TaskDescriptor
    {
        model::Task::Status status = model::Task::Status::draft;
        std::string kvnrPatient = InsurantA;
        std::optional<std::string> telematicIdPharmacy = std::nullopt;
        std::string accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));
        model::PrescriptionType prescriptionType = model::PrescriptionType::apothekenpflichigeArzneimittel;
        std::optional<model::Timestamp> expiryDate = std::nullopt;
    };

    virtual std::vector<model::Task> addTasksToDatabase(const std::vector<TaskDescriptor>& descriptors);
    virtual model::Task addTaskToDatabase(const TaskDescriptor& descriptor);

    virtual model::Task createTask(
        const std::string& accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32)),
        model::PrescriptionType prescriptionType = model::PrescriptionType::apothekenpflichigeArzneimittel);
    virtual void activateTask(model::Task& task, const std::string& kvnrPatient = InsurantE,
                              std::optional<model::Timestamp> expiryDate = std::nullopt,
                              std::optional<ResourceTemplates::KbvBundleMvoOptions> mvoOptions = std::nullopt);
    virtual void acceptTask(model::Task& task, const SafeString secret = SecureRandomGenerator::generate(32));
    virtual std::vector<model::MedicationDispense> closeTask(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const std::optional<model::Timestamp>& medicationWhenPrepared = std::nullopt,
        size_t numMedications = 1);
    virtual void abortTask(model::Task& task);

    struct CommunicationAttandee
    {
        ActorRole role;
        std::string name;
    };
    struct CommunicationDescriptor
    {
        std::optional<model::PrescriptionId> prescriptionId;
        model::Communication::MessageType messageType;
        CommunicationAttandee sender;
        CommunicationAttandee recipient = CommunicationAttandee{ActorRole::Insurant, ""};
        std::string accessCode;
        std::optional<std::string> contentString = "";
        std::optional<model::Timestamp> sent = model::Timestamp::now();
        std::optional<model::Timestamp> received = std::nullopt;
    };

    virtual std::vector<Uuid> addCommunicationsToDatabase(const std::vector<CommunicationDescriptor>& descriptors);
    virtual model::Communication addCommunicationToDatabase(const CommunicationDescriptor& descriptor);

    struct ChargeItemDescriptor
    {
        model::PrescriptionId prescriptionId;
        std::string_view kvnrStr;
        std::string_view accessCode;
        std::string_view telematikId;
        std::string_view healthCarePrescriptionUuid;
    };
    virtual model::ChargeItem addChargeItemToDatabase(const ChargeItemDescriptor& descriptor);

    std::unique_ptr<HttpsServer> mServer;

protected:
    model::MedicationDispense createMedicationDispense(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const model::Timestamp& whenHandedOver = model::Timestamp::now(),
        const std::variant<std::monostate, model::Timestamp, std::string>& whenPrepared = std::monostate{}) const;

    // Jwt of InsurantF ("X234567891"):
    std::unique_ptr<JWT> mJwt;
    std::unique_ptr<MockDatabase> mMockDatabase;
    std::unique_ptr<HsmPool> mHsmPool;

    ClientTeeProtocol mTeeProtocol;
    const bool mHasPostgresSupport;
    JwtBuilder mJwtBuilder;

    // Valid access code which may be used to create and reference tasks.
    static constexpr const char* TaskAccessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";

    // Please note that for a doctor or a pharmacy the "JwtBuilder.makeJwt<>" methods are
    // retrieving the names from json files ("claims_arzt.json" or "claims_apotheke.json").
    // For our tests one doctor and one pharmacist is sufficient to handle most test cases.
    // Names of doctors and pharmacies may have up to 256 characters.
    // In contrary several tests need different insurants so here a set of predefined
    // insurant names is provided. The KVNRs of insurants is limited to 10 characters.
    static constexpr const char* InsurantA = "X100000013";
    static constexpr const char* InsurantB = "X100000025";
    static constexpr const char* InsurantC = "X100000037";
    static constexpr const char* InsurantD = "X100000049";
    static constexpr const char* InsurantE = "X123456788";
    static constexpr const char* InsurantF = "A123457895";
    static constexpr const char* InsurantG = "X234567891";
    static constexpr const char* InsurantH = "X234567802";

    // eGK test card identities based on the formation rule in Card-G2-A_3820
    // (X0000nnnnP, with nnnn from the set {0001 .. 5000} and P = Check digit)
    static constexpr const char* VerificationIdentityKvnrMin = "X000000012";
    static constexpr const char* VerificationIdentityKvnrAny = "X000025003";
    static constexpr const char* VerificationIdentityKvnrMax = "X000050005";

    // The names of the pharmacy and doctor will be retrieved from the JWTs.
    model::TelematikId mPharmacy;
    model::TelematikId mDoctor;

    std::unique_ptr<PcServiceContext> mContext;

    Database::Factory createDatabaseFactory (void);
    std::unique_ptr<Database> createDatabase (void);
    virtual void addAdditionalPrimaryHandlers (RequestHandlerManager&) {}
    virtual void addAdditionalSecondaryHandlers (RequestHandlerManager&) {}

    /**
     * At the moment this method is idempotent. That means the server is started once per test suite, not per test.
     */
    void startServer (void);

    void writeCurrentTestOutputFile(
        const std::string& testOutput,
        const std::string& fileExtension,
        const std::string& marker = std::string());

private:
    static bool isPostgresEnabled();

    std::unique_ptr<pqxx::connection> mConnection;

    void initializeIdp (Idp& idp);
};


#endif
