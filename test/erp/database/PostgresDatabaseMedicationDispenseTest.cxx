/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/database/PostgresDatabaseMedicationDispenseTest.hxx"

#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <chrono>

using namespace std::chrono_literals;

using namespace model;


std::set<int64_t> PostgresDatabaseMedicationDispenseTest::sTaskPrescriptionIdsToBeDeleted;

PostgresDatabaseMedicationDispenseTest::PostgresDatabaseMedicationDispenseTest() :
    PostgresDatabaseTest()
{
    auto profileVersion = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    bool isDeprecated = model::ResourceVersion::deprecatedProfile(profileVersion);
    mNsPrescriptionId = isDeprecated ? model::resource::naming_system::deprecated::prescriptionID : model::resource::naming_system::prescriptionID;
}

void PostgresDatabaseMedicationDispenseTest::cleanup()
{
    if (usePostgres())
    {
        clearTables();
    }
}

/**
    @Param: (IN) patientsPharmaciesMedicationWhenPrepared
        Array with tuples containing three elements:
        0: KVNR of patient
        1: Telematic Id of pharmacy
        2: (optional) Timestamp when medication has been prepared

    @Param: (OUT) patients
        Vector with patients

    @Param: (OUT) pharmacies
        Vector with pharmacies

    @Param: (OUT) tasksByPrescriptionIds
        Map with prescriptionId as key and Tasks as elements.

    @Param: (OUT) medicationDispensesByPrescriptionIds
        Map with prescriptionId as key and MedicationDispenses as elements.

    @Param: (OUT) prescriptionIdsByPatients
        Map with kvnr of patient as key and vector with prescriptionId of tasks and MedicationDispenses.

    @Param: (OUT) prescriptionIdsByPharmacies
        Map with telematikId of pharmacy as key and vector with prescriptionId of tasks and MedicationDispenses.

    @Param: (OUT) medicationDispensesInputXmlStrings
        The order in which the medication dispenses is returned is unpredictable.
        They will be sorted according to their ids.
*/
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void PostgresDatabaseMedicationDispenseTest::insertTasks(
    const std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<Timestamp>>>& patientsPharmaciesMedicationWhenPrepared,
    std::set<std::string>& patients,
    std::set<std::string>& pharmacies,
    std::map<std::string, Task>& tasksByPrescriptionIds,
    std::map<std::string, std::vector<MedicationDispense>>& medicationDispensesByPrescriptionIds,
    std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
    std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
    std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
    model::PrescriptionType prescriptionType)
{
    for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
    {
        auto kvnrPatient = std::get<0>(patientAndPharmacy);
        auto pharmacy = std::get<1>(patientAndPharmacy);
        std::optional<Timestamp> whenPrepared = std::get<2>(patientAndPharmacy);

        Task task = createAcceptedTask(kvnrPatient.id(), prescriptionType);
        auto medicationDispenses = closeTask(task, pharmacy.id(), whenPrepared);
        EXPECT_GT(medicationDispenses.size(), 0);
        const auto& medicationDispense = medicationDispenses[0];

        PrescriptionId prescriptionId = task.prescriptionId();

        PrescriptionId medicationDispenseId = medicationDispense.prescriptionId();
        ASSERT_EQ(medicationDispenseId, prescriptionId);

        auto medicationDispenseKvnr = medicationDispense.kvnr();
        ASSERT_EQ(medicationDispenseKvnr, kvnrPatient);

        auto medicationDispenseTelematicId = medicationDispense.telematikId();
        ASSERT_EQ(medicationDispenseTelematicId, pharmacy);

        std::chrono::system_clock::time_point handedOver = medicationDispense.whenHandedOver().toChronoTimePoint();
        std::chrono::system_clock::time_point now = Timestamp::now().toChronoTimePoint();
        ASSERT_TRUE(handedOver > now - 10s && handedOver < now + 10s);

        std::optional<Timestamp> medicationDispenseWhenPrepared = medicationDispense.whenPrepared();
        ASSERT_EQ(medicationDispenseWhenPrepared, whenPrepared);

        for (const auto& item : medicationDispenses)
        {
            std::string xmlString = item.serializeToXmlString();
            medicationDispensesInputXmlStrings.insert(std::make_pair(item.id().toString(), xmlString));
        }

        patients.insert(kvnrPatient.id());
        pharmacies.insert(pharmacy.id());

        tasksByPrescriptionIds.insert(std::make_pair(prescriptionId.toString(), std::move(task)));
        medicationDispensesByPrescriptionIds.insert(std::make_pair(prescriptionId.toString(), std::move(medicationDispenses)));

        if (prescriptionIdsByPatients.find(kvnrPatient.id()) == prescriptionIdsByPatients.end())
        {
            prescriptionIdsByPatients.insert(std::make_pair(kvnrPatient.id(), std::vector<std::string>()));
        }
        prescriptionIdsByPatients[kvnrPatient.id()].push_back(prescriptionId.toString());

        if (prescriptionIdsByPharmacies.find(pharmacy.id()) == prescriptionIdsByPharmacies.end())
        {
            prescriptionIdsByPharmacies.insert(std::make_pair(pharmacy.id(), std::vector<std::string>()));
        }
        prescriptionIdsByPharmacies[pharmacy.id()].push_back(prescriptionId.toString());
    }
}

UrlArguments PostgresDatabaseMedicationDispenseTest::createSearchArguments(ServerRequest::QueryParametersType&& queryParameters)
{
    ServerRequest request = ServerRequest(Header());
    request.setQueryParameters(std::move(queryParameters));

    std::vector<SearchParameter> searchParameters = {
        { "performer", "performer", SearchParameter::Type::HashedIdentity },
        { "whenhandedover", "when_handed_over", SearchParameter::Type::Date },
        { "whenprepared", "when_prepared", SearchParameter::Type::Date },
        { "identifier", "prescription_id", SearchParameter::Type::PrescriptionId}
    };

    UrlArguments searchArgs = UrlArguments(std::move(searchParameters));
    searchArgs.parse(request, getKeyDerivation());

    return searchArgs;
}

void PostgresDatabaseMedicationDispenseTest::checkMedicationDispensesXmlStrings(
    std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
    const std::vector<MedicationDispense>& medicationDispenses)
{
    for (const auto& medicationDispense : medicationDispenses)
    {
        std::string medicationDispenseResponseXmlString = medicationDispense.serializeToXmlString();
        std::string medicationDispenseId(medicationDispense.id().toString());

        auto dispenseInput = medicationDispensesInputXmlStrings.find(medicationDispenseId);
        if (dispenseInput != medicationDispensesInputXmlStrings.end())
        {
            EXPECT_EQ(medicationDispenseResponseXmlString, dispenseInput->second);
        }
    }
}

void PostgresDatabaseMedicationDispenseTest::writeCurrentTestOutputFile(
    const std::string& testOutput,
    const std::string& fileExtension,
    const std::string& marker) const
{
    if (!writeTestOutputFileEnabled)
        return;
    const ::testing::TestInfo* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string testName = testInfo->name();
    std::string testCaseName = testInfo->test_case_name();
    std::string fileName = testCaseName + "-" + testName;
    if (!marker.empty())
        fileName += marker;
    fileName += "." + fileExtension;
    FileHelper::writeFile(fileName, testOutput);
}

Task PostgresDatabaseMedicationDispenseTest::createAcceptedTask(const std::string_view& kvnrPatient, model::PrescriptionType prescriptionType)
{
    Task task = createTask(prescriptionType);
    task.setKvnr(model::Kvnr{kvnrPatient, task.prescriptionId().isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv});
    activateTask(task);
    acceptTask(task);
    return task;
}

std::vector<MedicationDispense> PostgresDatabaseMedicationDispenseTest::closeTask(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const std::optional<Timestamp>& medicationWhenPrepared)
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
    ::model::Binary prescriptionDigestResource{"TestDigest", "Test", ::model::Binary::Type::Digest};

    task.setReceiptUuid();
    task.updateLastUpdate();

    std::vector<model::MedicationDispense> medicationDispenses;
    for(size_t i = 0; i < GetParam().numMedications; ++i)
    {
        medicationDispenses.emplace_back(
            createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, medicationWhenPrepared));
        medicationDispenses.back().setId(model::MedicationDispenseId(medicationDispenses.back().prescriptionId(), i));
    }

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/", prescriptionId,
        compositionResource, authorIdentifier, deviceResource, "TestDigest", prescriptionDigestResource);

    database().updateTaskMedicationDispenseReceipt(task, medicationDispenses, responseReceipt);
    database().commitTransaction();

    return medicationDispenses;
}

MedicationDispense PostgresDatabaseMedicationDispenseTest::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::optional<Timestamp>& whenPrepared)
{
    const auto kvnr = task.kvnr().value().id();
    const ResourceTemplates::MedicationDispenseOptions settings {
        .prescriptionId = task.prescriptionId(),
        .kvnr = kvnr,
        .telematikId = telematicIdPharmacy,
        .whenHandedOver = whenHandedOver,
        .whenPrepared = whenPrepared };
    const auto medicationDispenseXmlString = ResourceTemplates::medicationDispenseXml(settings);

    MedicationDispense medicationDispense = MedicationDispense::fromXmlNoValidation(medicationDispenseXmlString);
    return medicationDispense;
}

Task PostgresDatabaseMedicationDispenseTest::createTask(PrescriptionType prescriptionType)
{
    const std::string accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));

    Task task(prescriptionType, accessCode);

    PrescriptionId prescriptionId = database().storeTask(task);
    task.setPrescriptionId(prescriptionId);

    database().commitTransaction();
    sTaskPrescriptionIdsToBeDeleted.insert(prescriptionId.toDatabaseId());

    return task;
}

void PostgresDatabaseMedicationDispenseTest::activateTask(Task& task)
{
    ASSERT_TRUE(task.kvnr().has_value());

    PrescriptionId prescriptionId = task.prescriptionId();

    std::string kvnrPatient = std::string(task.kvnr().value().id());

    Timestamp signingTime = Timestamp::now();

    const auto prescriptionBundleXmlString = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .kvnr = kvnrPatient});

    const auto prescriptionBundle = KbvBundle::fromXmlNoValidation(prescriptionBundleXmlString);

    const std::vector<Patient> patients = prescriptionBundle.getResourcesByType<Patient>("Patient");

    ASSERT_FALSE(patients.empty());

    for (const auto& patient : patients)
    {
        ASSERT_EQ(patient.kvnr(), kvnrPatient);
    }

    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setExpiryDate(signingTime + std::chrono::hours(24) * 92);
    task.setAcceptDate(signingTime + std::chrono::hours(24) * 92);
    task.setStatus(model::Task::Status::ready);
    task.updateLastUpdate();

    const auto& pemFilename = TestConfiguration::instance().getOptionalStringValue(
        TestConfigurationKey::TEST_QES_PEM_FILE_NAME ).value_or(std::string{TEST_DATA_DIR} + "/qes.pem");
    auto pem_str = ResourceManager::instance().getStringResource(pemFilename);
    auto cert = Certificate::fromPem(pem_str);
    SafeString pem{std::move(pem_str)};
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(pem);
    CadesBesSignature cadesBesSignature{ cert, privKey, prescriptionBundleXmlString };
    std::string encodedPrescriptionBundleXmlString = cadesBesSignature.getBase64();
    const Binary healthCareProviderPrescriptionBinary(*task.healthCarePrescriptionUuid(), encodedPrescriptionBundleXmlString);

    database().activateTask(task, healthCareProviderPrescriptionBinary);
    database().commitTransaction();
}

void PostgresDatabaseMedicationDispenseTest::acceptTask(model::Task& task)
{
    task.setStatus(model::Task::Status::inprogress);
    const SafeString secret = SecureRandomGenerator::generate(32);
    task.setSecret(ByteHelper::toHex(secret));
    task.updateLastUpdate();

    database().updateTaskStatusAndSecret(task);
    database().commitTransaction();
}

void PostgresDatabaseMedicationDispenseTest::deleteTaskByPrescriptionId(const int64_t prescriptionId)
{
    auto transaction = createTransaction();
    const pqxx::result result = transaction.exec("DELETE FROM erp.task WHERE prescription_id = " + std::to_string(prescriptionId));
    transaction.commit();
    sTaskPrescriptionIdsToBeDeleted.erase(prescriptionId);
}


TEST_P(PostgresDatabaseMedicationDispenseTest, OneTaskGetAllNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    // Insert task into database
    //--------------------------

    auto kvnrPatient = model::Kvnr{InsurantA};
    auto pharmacy = model::TelematikId{"3-SMC-B-Testkarte-883110000120312"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    const auto [medicationDispenses, _] =
        database().retrieveAllMedicationDispenses(kvnrPatient, std::optional<UrlArguments>{});
    database().commitTransaction();
    EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
    checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
}

TEST_P(PostgresDatabaseMedicationDispenseTest, OneTaskGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    // Insert task into database
    //--------------------------

    auto kvnrPatient = model::Kvnr{InsurantA};
    auto pharmacy = model::TelematikId{"3-SMC-B-Testkarte-883110000120312"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy.id()} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", "3-SMC-B-Ungueltig-123455678909876"} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy.id()},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        const auto& prescriptionId = prescriptionIdsByPatients[kvnrPatient.id()].at(0);
        ServerRequest::QueryParametersType queryParameters{
            {"identifier", std::string(mNsPrescriptionId) + "|" + prescriptionId}};
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_P(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetAllNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    auto pharmacyA = model::TelematikId{"3-SMC-B-Testkarte-883110000120312"};
    auto pharmacyB = model::TelematikId{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantA}, pharmacyB, std::nullopt},
        {model::Kvnr{InsurantB}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantC}, pharmacyB, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(model::Kvnr{kvnrPatient}, std::optional<UrlArguments>{});
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), prescriptionIds.size() * GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_P(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    auto pharmacyA = model::TelematikId{"3-SMC-B-Testkarte-883110000120312"};
    auto pharmacyB = model::TelematikId{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {model::Kvnr{InsurantA}, pharmacyA, Timestamp::now()},
        {model::Kvnr{InsurantA}, pharmacyB, std::nullopt},
        {model::Kvnr{InsurantB}, pharmacyA, Timestamp::now()},
        {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantC}, pharmacyB, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 3 * GetParam().numMedications;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = GetParam().numMedications;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyA.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 2 * GetParam().numMedications;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantC};
        std::string pharmacy = pharmacyA.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyB.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = GetParam().numMedications;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyA.id();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = GetParam().numMedications;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto [medicationDispenses, _] =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        const auto& prescriptionIds = prescriptionIdsByPatients[InsurantA];
        for (const auto& prescriptionId : prescriptionIds)
        {
            ServerRequest::QueryParametersType queryParameters{
                {"identifier", std::string(mNsPrescriptionId) + "|" + prescriptionId}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const auto [medicationDispenses, _] =
                database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_P(PostgresDatabaseMedicationDispenseTest, ManyTasksGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    model::TelematikId pharmacyA{"3-SMC-B-Testkarte-883110000120312"};
    model::TelematikId pharmacyB{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared;

    for (size_t idxPatient = 0; idxPatient < 70; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantA}, pharmacyA, Timestamp::now());
    }
    for (size_t idxPatient = 0; idxPatient < 10; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantA}, pharmacyB, Timestamp::now());
    }
    for (size_t idxPatient = 0; idxPatient < 15; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantB}, pharmacyA, Timestamp::now());
    }
    for (size_t idxPatient = 0; idxPatient < 55; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantB}, pharmacyB, Timestamp::now());
    }
    for (size_t idxPatient = 0; idxPatient < 5; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantC}, pharmacyA, Timestamp::now());
    }
    for (size_t idxPatient = 0; idxPatient < 5; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.emplace_back(model::Kvnr{InsurantC}, pharmacyB, Timestamp::now());
    }

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
                tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
                prescriptionIdsByPatients, prescriptionIdsByPharmacies,
                medicationDispensesInputXmlStrings, model::PrescriptionType::apothekenpflichigeArzneimittel);
    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
                tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
                prescriptionIdsByPatients, prescriptionIdsByPharmacies,
                medicationDispensesInputXmlStrings, model::PrescriptionType::direkteZuweisung);

    // GET Medication Dispenses
    //-------------------------

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyA.id();

        size_t expectedCountAll = 0;

        for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
        {
            if ((std::get<0>(patientAndPharmacy) == kvnrPatient)
             && (std::get<1>(patientAndPharmacy) == pharmacy))
            {
                ++expectedCountAll;
            }
        }

        expectedCountAll *= 2 * GetParam().numMedications;

        // Paging.
        ASSERT_TRUE(expectedCountAll > 50);

        size_t expectedCount = 50 * GetParam().numMedications;
        size_t receivedCount = 0;

        while (receivedCount < expectedCountAll)
        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
            ServerRequest::QueryParametersType queryParameters{
                {"performer", pharmacy},
                {"whenhandedover", "gt" + whenHandedOver},
                {"whenprepared", "gt" + whenPrepared},
                {"_sort", "-whenhandedover"},
                {"_count", std::to_string(50)},
                {"__offset", std::to_string(receivedCount/GetParam().numMedications)}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const auto [medicationDispenses, _] =
                database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
            database().commitTransaction();
            // ASSERT to avoid endless loop if less than expected count is returned.
            ASSERT_EQ(medicationDispenses.size(), expectedCount);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
            if (!medicationDispenses.empty())
            {
                for (size_t idx = 0; idx < medicationDispenses.size() - 1; ++idx)
                {
                    const auto& medicationDispenseThis = medicationDispenses[idx];
                    const auto& medicationDispenseNext = medicationDispenses[idx + 1];
                    ASSERT_GE(medicationDispenseThis.whenHandedOver(), medicationDispenseNext.whenHandedOver());
                }
            }
            receivedCount += medicationDispenses.size();
            expectedCount = std::min(expectedCountAll - receivedCount, 50*GetParam().numMedications);
        }
    }

    for (const std::string& kvnrPatientId : patients)
    {
        model::Kvnr kvnrPatient{kvnrPatientId};
        // whenhandedover is in the future
        std::string pharmacy = pharmacyA.id();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatientId : patients)
    {
        // whenprepared is in the future
        model::Kvnr kvnrPatient{kvnrPatientId};
        std::string pharmacy = pharmacyA.id();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatientId : patients)
    {
        model::Kvnr kvnrPatient{kvnrPatientId};
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto [medicationDispenses, _] = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        const auto& prescriptionIds = prescriptionIdsByPatients[InsurantA];
        for (const auto& prescriptionId : prescriptionIds)
        {
            ServerRequest::QueryParametersType queryParameters{
                {"identifier", std::string(mNsPrescriptionId) + "|" + prescriptionId}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const auto [medicationDispenses, _] =
                database().retrieveAllMedicationDispenses(model::Kvnr{InsurantA}, searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), GetParam().numMedications);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_P(PostgresDatabaseMedicationDispenseTest, OneTaskGetByIdNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert task into database
    //--------------------------

    auto kvnrPatient = model::Kvnr{InsurantA};
    auto pharmacy = model::TelematikId{"3-SMC-B-Testkarte-883110000120312"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient.id())->second;

    for (const std::string& prescriptionId : prescriptionIds)
    {
        for (size_t i = 0; i < GetParam().numMedications; ++i)
        {
            model::MedicationDispenseId id(model::PrescriptionId::fromString(prescriptionId), i);
            const auto medicationDispense = database().retrieveMedicationDispense(kvnrPatient, id);
            database().commitTransaction();
            EXPECT_TRUE(medicationDispense.has_value());
            EXPECT_EQ(medicationDispense->serializeToXmlString(),
                      medicationDispensesInputXmlStrings[std::string(medicationDispense->id().toString())]);
        }
    }
}

TEST_P(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetByIdNoFilter)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    model::TelematikId pharmacyA{"3-SMC-B-Testkarte-883110000120312"};
    model::TelematikId pharmacyB{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantA}, pharmacyB, std::nullopt},
        {model::Kvnr{InsurantB}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
        {model::Kvnr{InsurantC}, pharmacyB, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    for (const std::string& kvnrPatientId : patients)
    {
        const model::Kvnr kvnrPatient{kvnrPatientId};
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatientId)->second;

        for (const std::string& prescriptionId : prescriptionIds)
        {
            for (size_t i = 0; i < GetParam().numMedications; ++i)
            {
                model::MedicationDispenseId id(model::PrescriptionId::fromString(prescriptionId), i);
                const auto medicationDispense = database().retrieveMedicationDispense(kvnrPatient, id);
                database().commitTransaction();
                EXPECT_TRUE(medicationDispense.has_value());
                EXPECT_EQ(medicationDispense->serializeToXmlString(),
                          medicationDispensesInputXmlStrings[std::string(medicationDispense->id().toString())]);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(PostgresDatabaseMedicationDispenseTestInst, PostgresDatabaseMedicationDispenseTest,
                         testing::Values(PostgresDatabaseMedicationDispenseTestParams{1, model::PrescriptionType::apothekenpflichigeArzneimittel},
                                         PostgresDatabaseMedicationDispenseTestParams{4, model::PrescriptionType::direkteZuweisung},
                                         PostgresDatabaseMedicationDispenseTestParams{10, model::PrescriptionType::apothekenpflichigeArzneimittel},
                                         PostgresDatabaseMedicationDispenseTestParams{3, model::PrescriptionType::direkteZuweisungPkv}));

std::ostream& operator<<(std::ostream& os, const PostgresDatabaseMedicationDispenseTestParams& params)
{
    os << "numMedications: " << params.numMedications << " type: " << params.type;
    return os;
}
