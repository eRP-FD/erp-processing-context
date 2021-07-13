#include "test/erp/database/PostgresDatabaseMedicationDispenseTest.hxx"

#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/StaticData.hxx"

#include <chrono>

using namespace std::chrono_literals;

using namespace model;


std::set<int64_t> PostgresDatabaseMedicationDispenseTest::sTaskPrescriptionIdsToBeDeleted;

PostgresDatabaseMedicationDispenseTest::PostgresDatabaseMedicationDispenseTest() :
    PostgresDatabaseTest()
{
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
void PostgresDatabaseMedicationDispenseTest::insertTasks(
    const std::vector<std::tuple<std::string, std::string, std::optional<Timestamp>>>& patientsPharmaciesMedicationWhenPrepared,
    std::set<std::string>& patients,
    std::set<std::string>& pharmacies,
    std::map<std::string, Task>& tasksByPrescriptionIds,
    std::map<std::string, MedicationDispense>& medicationDispensesByPrescriptionIds,
    std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
    std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
    std::map<std::string, std::string>& medicationDispensesInputXmlStrings)
{
    int idxTask = 0;

    for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
    {
        std::string kvnrPatient = std::get<0>(patientAndPharmacy);
        std::string pharmacy = std::get<1>(patientAndPharmacy);
        std::optional<Timestamp> whenPrepared = std::get<2>(patientAndPharmacy);

        Task task = createAcceptedTask(kvnrPatient);
        MedicationDispense medicationDispense = closeTask(task, pharmacy, whenPrepared);

        PrescriptionId prescriptionId = task.prescriptionId();

        PrescriptionId medicationDispenseId = medicationDispense.id();
        ASSERT_EQ(medicationDispenseId, prescriptionId);

        std::string_view medicationDispenseKvnr = medicationDispense.kvnr();
        ASSERT_EQ(medicationDispenseKvnr, kvnrPatient);

        std::string_view medicationDispenseTelematicId = medicationDispense.telematikId();
        ASSERT_EQ(medicationDispenseTelematicId, pharmacy);

        std::chrono::system_clock::time_point handedOver = medicationDispense.whenHandedOver().toChronoTimePoint();
        std::chrono::system_clock::time_point now = Timestamp::now().toChronoTimePoint();
        ASSERT_TRUE(handedOver > now - 10s && handedOver < now + 10s);

        std::optional<Timestamp> medicationDispenseWhenPrepared = medicationDispense.whenPrepared();
        ASSERT_EQ(medicationDispenseWhenPrepared, whenPrepared);

        std::string xmlString = medicationDispense.serializeToXmlString();
        medicationDispensesInputXmlStrings.insert(std::make_pair(prescriptionId.toString(), xmlString));

        patients.insert(kvnrPatient);
        pharmacies.insert(pharmacy);

        tasksByPrescriptionIds.insert(std::make_pair(prescriptionId.toString(), std::move(task)));
        medicationDispensesByPrescriptionIds.insert(std::make_pair(prescriptionId.toString(), std::move(medicationDispense)));

        if (prescriptionIdsByPatients.find(kvnrPatient) == prescriptionIdsByPatients.end())
        {
            prescriptionIdsByPatients.insert(std::make_pair(kvnrPatient, std::vector<std::string>()));
        }
        prescriptionIdsByPatients[kvnrPatient].push_back(prescriptionId.toString());

        if (prescriptionIdsByPharmacies.find(pharmacy) == prescriptionIdsByPharmacies.end())
        {
            prescriptionIdsByPharmacies.insert(std::make_pair(pharmacy, std::vector<std::string>()));
        }
        prescriptionIdsByPharmacies[pharmacy].push_back(prescriptionId.toString());

        ++idxTask;
    }
}

UrlArguments PostgresDatabaseMedicationDispenseTest::createSearchArguments(ServerRequest::QueryParametersType&& queryParameters)
{
    ServerRequest request = ServerRequest(Header());
    request.setQueryParameters(std::move(queryParameters));

    std::vector<SearchParameter> searchParameters = {
        { "performer", "performer", SearchParameter::Type::HashedIdentity },
        { "whenhandedover", "when_handed_over", SearchParameter::Type::Date },
        { "whenprepared", "when_prepared", SearchParameter::Type::Date }
    };

    UrlArguments searchArgs = UrlArguments(std::move(searchParameters));
    searchArgs.parse(request, getKeyDerivation());

    return searchArgs;
}

void PostgresDatabaseMedicationDispenseTest::checkMedicationDispensesXmlStrings(
    std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
    const std::vector<MedicationDispense>& medicationDispenses)
{
    int idxResource = 0;
    for (const auto& medicationDispense : medicationDispenses)
    {
        std::string medicationDispenseResponseXmlString = medicationDispense.serializeToXmlString();
        std::optional<PrescriptionId> medicationDispenseId = medicationDispense.id();
        ASSERT_TRUE(medicationDispenseId.has_value());

        auto dispenseInput = medicationDispensesInputXmlStrings.find(medicationDispenseId.value().toString());
        if (dispenseInput != medicationDispensesInputXmlStrings.end())
        {
            EXPECT_EQ(medicationDispenseResponseXmlString, dispenseInput->second);
        }
        ++idxResource;
    }
}

void PostgresDatabaseMedicationDispenseTest::writeCurrentTestOutputFile(
    const std::string& testOutput,
    const std::string& fileExtension,
    const std::string& marker)
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

Task PostgresDatabaseMedicationDispenseTest::createAcceptedTask(const std::string_view& kvnrPatient)
{
    Task task = createTask();
    task.setKvnr(kvnrPatient);
    activateTask(task);
    acceptTask(task);
    return task;
}

MedicationDispense PostgresDatabaseMedicationDispenseTest::closeTask(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const std::optional<Timestamp>& medicationWhenPrepared)
{
    PrescriptionId prescriptionId = task.prescriptionId();

    const Timestamp inProgessDate = task.lastModifiedDate();
    const Timestamp completedTimestamp = model::Timestamp::now();
    const std::string linkBase = "https://127.0.0.1:8080";
    const std::string authorIdentifier = linkBase + "/Device";
    Composition compositionResource(telematicIdPharmacy, inProgessDate, completedTimestamp, authorIdentifier);
    Device deviceResource;

    task.setReceiptUuid();
    task.updateLastUpdate();

    MedicationDispense medicationDispense =
        createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, medicationWhenPrepared);

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/",
        prescriptionId, compositionResource, authorIdentifier, deviceResource);

    database().updateTaskMedicationDispenseReceipt(task, medicationDispense, responseReceipt);
    database().commitTransaction();

    return medicationDispense;
}

MedicationDispense PostgresDatabaseMedicationDispenseTest::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::optional<Timestamp>& whenPrepared)
{
    std::string medicationDispenseXmlString =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/fhir/conversion/medication_dispense.xml");

    MedicationDispense medicationDispense = MedicationDispense::fromXml(
    medicationDispenseXmlString,
        *StaticData::getXmlValidator(),
        SchemaType::Gem_erxMedicationDispense);

    PrescriptionId prescriptionId = task.prescriptionId();
    std::string_view kvnrPatient = task.kvnr().value();

    medicationDispense.setId(prescriptionId);
    medicationDispense.setKvnr(kvnrPatient);
    medicationDispense.setTelematicId(telematicIdPharmacy);
    medicationDispense.setWhenHandedOver(whenHandedOver);

    // The element "whenPrepared" is optional
    if (whenPrepared.has_value())
    {
        medicationDispense.setWhenPrepared(whenPrepared.value());
    }

    return medicationDispense;
}

Task PostgresDatabaseMedicationDispenseTest::createTask()
{
    const std::string accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));
    PrescriptionType prescriptionType = PrescriptionType::apothekenpflichigeArzneimittel;
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

    std::string kvnrPatient = std::string(task.kvnr().value());

    Timestamp signingTime = Timestamp::now();

    std::string prescriptionBundleXmlString = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/MedicationDispenseKbvBundle1.xml");

    prescriptionBundleXmlString = String::replaceAll(prescriptionBundleXmlString, "##PrescriptionId##", prescriptionId.toString());
    prescriptionBundleXmlString = String::replaceAll(prescriptionBundleXmlString, "##kvnrPatient##", kvnrPatient);

    Bundle prescriptionBundle = Bundle::fromXmlNoValidation(prescriptionBundleXmlString);

    const std::vector<Patient> patients = prescriptionBundle.getResourcesByType<Patient>("Patient");

    ASSERT_TRUE(patients.size() > 0);

    for (const auto& patient : patients)
    {
        ASSERT_EQ(patient.getKvnr(), kvnrPatient);
    }

    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setExpiryDate(signingTime + std::chrono::hours(24) * 92);
    task.setAcceptDate(signingTime + std::chrono::hours(24) * 92);
    task.setStatus(model::Task::Status::ready);
    task.updateLastUpdate();

    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{ ErpWorkflowTest::privateKey });
    auto cert = Certificate::fromPemString(ErpWorkflowTest::certificate);
    CadesBesSignature cadesBesSignature{ cert, privKey, prescriptionBundleXmlString };
    std::string encodedPrescriptionBundleXmlString = Base64::encode(cadesBesSignature.get());
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


TEST_F(PostgresDatabaseMedicationDispenseTest, OneTaskGetAllNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    // Insert task into database
    //--------------------------

    std::string kvnrPatient = InsurantA;
    std::string pharmacy = "3-SMC-B-Testkarte-883110000120312";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    const std::vector<MedicationDispense> medicationDispenses =
        database().retrieveAllMedicationDispenses(kvnrPatient, {}, {});
    database().commitTransaction();
    EXPECT_EQ(medicationDispenses.size(), 1);
    checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
}

TEST_F(PostgresDatabaseMedicationDispenseTest, OneTaskGetAllSeveralFilters)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    // Insert task into database
    //--------------------------

    std::string kvnrPatient = InsurantA;
    std::string pharmacy = "3-SMC-B-Testkarte-883110000120312";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", "3-SMC-B-Ungueltig-123455678909876"} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetAllNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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

        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, {});
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), prescriptionIds.size());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetAllSeveralFilters)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {InsurantA, pharmacyA, Timestamp::now()},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, Timestamp::now()},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 3;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyA;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 2;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantC;
        std::string pharmacy = pharmacyA;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyB;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, ManyTasksGetAllSeveralFilters)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared;

    for (size_t idxPatient = 0; idxPatient < 70; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantA, pharmacyA, Timestamp::now()));
    }
    for (size_t idxPatient = 0; idxPatient < 10; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantA, pharmacyB, Timestamp::now()));
    }
    for (size_t idxPatient = 0; idxPatient < 15; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantB, pharmacyA, Timestamp::now()));
    }
    for (size_t idxPatient = 0; idxPatient < 55; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantB, pharmacyB, Timestamp::now()));
    }
    for (size_t idxPatient = 0; idxPatient < 5; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantC, pharmacyA, Timestamp::now()));
    }
    for (size_t idxPatient = 0; idxPatient < 5; ++idxPatient)
    {
        patientsPharmaciesMedicationWhenPrepared.push_back(std::make_tuple(InsurantC, pharmacyB, Timestamp::now()));
    }

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyA;

        size_t expectedCountAll = 0;

        for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
        {
            if ((std::get<0>(patientAndPharmacy) == kvnrPatient)
             && (std::get<1>(patientAndPharmacy) == pharmacy))
            {
                ++expectedCountAll;
            }
        }

        // Paging.
        ASSERT_TRUE(expectedCountAll > 50);

        size_t expectedCount = 50;
        size_t receivedCount = 0;

        while (receivedCount < expectedCountAll)
        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{
                {"performer", pharmacy},
                {"whenhandedover", "gt" + whenHandedOver},
                {"whenprepared", "gt" + whenPrepared},
                {"_sort", "-whenhandedover"},
                {"_count", std::to_string(50)},
                {"__offset", std::to_string(receivedCount)}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
            database().commitTransaction();
            // ASSERT to avoid endless loop if less than expected count is returned.
            ASSERT_EQ(medicationDispenses.size(), expectedCount);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
            if (medicationDispenses.size() > 0)
            {
                for (size_t idx = 0; idx < medicationDispenses.size() - 1; ++idx)
                {
                    const auto& medicationDispenseThis = medicationDispenses[idx];
                    const auto& medicationDispenseNext = medicationDispenses[idx + 1];
                    ASSERT_GE(medicationDispenseThis.whenHandedOver(), medicationDispenseNext.whenHandedOver());
                }
            }
            receivedCount += medicationDispenses.size();
            expectedCount = expectedCountAll - receivedCount;
            if (expectedCount > 50)
            {
                expectedCount = 50;
            }
        }
    }

    for (const std::string& kvnrPatient : patients)
    {
        // whenhandedover is in the future
        std::string pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatient : patients)
    {
        // whenprepared is in the future
        std::string pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatient : patients)
    {
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        size_t expectedCount = 0;
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            {"whenhandedover", "gt" + whenHandedOver},
            {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const std::vector<MedicationDispense> medicationDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, {}, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, OneTaskGetByIdNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert task into database
    //--------------------------

    std::string kvnrPatient = InsurantA;
    std::string pharmacy = "3-SMC-B-Testkarte-883110000120312";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

    for (const std::string& prescriptionId : prescriptionIds)
    {
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), {});
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, OneTaskGetByIdSeveralFilters)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert task into database
    //--------------------------

    std::string kvnrPatient = InsurantA;
    std::string pharmacy = "3-SMC-B-Testkarte-883110000120312";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {kvnrPatient, pharmacy, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

    for (const std::string& prescriptionId : prescriptionIds)
    {
        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }

        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 0);
        }

        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }

        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 0);
        }

        {
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }

        {
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 0);
        }

        {
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }

        {
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 0);
        }

        {
            ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }

        {
            ServerRequest::QueryParametersType queryParameters{ {"performer", "3-SMC-B-Ungueltig-123455678909876"} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 0);
        }

        {
            Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
            Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
            std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
            ServerRequest::QueryParametersType queryParameters{
                {"performer", pharmacy},
                {"whenhandedover", "gt" + whenHandedOver},
                {"whenprepared", "gt" + whenPrepared} };
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetByIdNoFilter)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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

        for (const std::string& prescriptionId : prescriptionIds)
        {
            const std::vector<MedicationDispense> medicationDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), {});
            database().commitTransaction();
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetByIdSeveralFilters)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {InsurantA, pharmacyA, Timestamp::now()},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, Timestamp::now()},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, Timestamp::now()}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
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
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = pharmacyA;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantC;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = pharmacyA;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = pharmacyB;
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0; // The first prescription id for Insurant A was for pharmacyA
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            { "whenhandedover", "gt" + whenHandedOver},
            { "whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 1;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string prescriptionId = prescriptionIdsByPatients[kvnrPatient].front();
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        ServerRequest::QueryParametersType queryParameters{
            {"performer", pharmacy},
            { "whenhandedover", "gt" + whenHandedOver},
            { "whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const std::vector<MedicationDispense> medicationDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, PrescriptionId::fromString(prescriptionId), searchArgs);
        database().commitTransaction();
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_F(PostgresDatabaseMedicationDispenseTest, SeveralTasksGetByIdUnknownId)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    // Insert tasks into database
    //---------------------------

    std::string pharmacyA = "3-SMC-B-Testkarte-883110000120312";
    std::string pharmacyB = "3-SMC-B-Testkarte-883110000120313";

    std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>> patientsPharmaciesMedicationWhenPrepared = {
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantA, pharmacyB, std::nullopt},
        {InsurantB, pharmacyA, std::nullopt},
        {InsurantA, pharmacyA, std::nullopt},
        {InsurantC, pharmacyB, std::nullopt}
    };

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, MedicationDispense> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies,
        tasksByPrescriptionIds, medicationDispensesByPrescriptionIds,
        prescriptionIdsByPatients, prescriptionIdsByPharmacies,
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    std::string kvnrPatient = InsurantA;

    PrescriptionId prescriptionId = PrescriptionId::fromString("160.000.000.004.711.86");

    ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacyA} };
    UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
    const std::vector<MedicationDispense> medicationDispenses =
        database().retrieveAllMedicationDispenses(kvnrPatient, prescriptionId, searchArgs);
    database().commitTransaction();
    EXPECT_EQ(medicationDispenses.size(), 0);
}
