/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASE_HXX


#include "test/erp/model/CommunicationTest.hxx"
#include "test/mock/ClientTeeProtocol.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/JsonTestUtils.hxx"

#include "erp/client/HttpsClient.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/KeyDerivation.hxx"
#include "erp/model/Task.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/HttpsServer.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Uuid.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>
#include <pqxx/transaction>

template<class T> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const T& jwt);

template<> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const JWT& jwt);
template<> std::pair<std::string, std::string> getAuthorizationHeaderForJwt(const std::string& jwt);

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
        "Hallo, wir haben das Medikament vorraetig. Kommen Sie gern in die Filiale oder wir schicken einen Boten.";
    const std::string DispReqMessage =
        "Bitte schicken Sie einen Boten.";
    const std::string RepresentativeMessageByInsurant =
        "Hallo, kannst Du das Medikament fuer mich holen?";
    const std::string RepresentativeMessageByRepresentative =
        "Ja, mache ich gerne.";
    constexpr static size_t serverThreadCount = 2;

    explicit ServerTestBase (bool forceMockDatabase = false);
    virtual ~ServerTestBase();

    void SetUp (void) override;
    void TearDown (void) override;

    HttpsClient createClient (void);

    virtual Header createPostHeader (const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual Header createGetHeader (const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual Header createDeleteHeader(const std::string& path, const std::optional<const JWT>& jwtToken = {}) const;
    virtual ClientRequest encryptRequest (const ClientRequest& innerRequest, const std::optional<const JWT>& jwtToken = {});
    virtual ClientResponse verifyOuterResponse (const ClientResponse& outerResponse);
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
    };

    virtual std::vector<model::Task> addTasksToDatabase(const std::vector<TaskDescriptor>& descriptors);
    virtual model::Task addTaskToDatabase(const TaskDescriptor& descriptor);

    virtual model::Task createTask(const std::string& accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32)));
    virtual void activateTask(model::Task& task, const std::string& kvnrPatient = InsurantE);
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
        std::string accessCode = "";
        std::optional<std::string> contentString = "";
        std::optional<model::Timestamp> sent = model::Timestamp::now();
        std::optional<model::Timestamp> received = std::nullopt;
    };

    virtual std::vector<Uuid> addCommunicationsToDatabase(const std::vector<CommunicationDescriptor>& descriptors);
    virtual model::Communication addCommunicationToDatabase(const CommunicationDescriptor& descriptor);

    std::unique_ptr<HttpsServer> mServer;

protected:
    model::MedicationDispense createMedicationDispense(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const model::Timestamp& whenHandedOver = model::Timestamp::now(),
        const std::optional<model::Timestamp>& whenPrepared = std::nullopt) const;

    // Jwt of InsurantF ("X234567890"):
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
    static constexpr const char* InsurantA = "X000000001";
    static constexpr const char* InsurantB = "X000000002";
    static constexpr const char* InsurantC = "X000000003";
    static constexpr const char* InsurantD = "X000000004";
    static constexpr const char* InsurantE = "X123456789"; // [task1.json, task2.json, task8.json]
    static constexpr const char* InsurantF = "X234567890"; // [task4.json, task5.json, task6.json, task7.json]
    static constexpr const char* InsurantG = "X234567891";
    static constexpr const char* InsurantH = "X234567892";

    // eGK test card identities based on the formation rule in Card-G2-A_3820
    // (X0000nnnnP, with nnnn from the set {0001 .. 5000} and P = Check digit)
    static constexpr const char* VerificationIdentityKvnrMin = "X000000017";
    static constexpr const char* VerificationIdentityKvnrAny = "X000025007";
    static constexpr const char* VerificationIdentityKvnrMax = "X000050007";

    // The names of the pharmacy and doctor will be retrieved from the JWTs.
    std::string mPharmacy;
    std::string mDoctor;

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
