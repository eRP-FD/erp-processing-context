/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/database/PostgresDatabaseMedicationDispenseTest.hxx"
#include "erp/model/EuMedicationDispenseInfos.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/model/Patient.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <shared/crypto/EllipticCurveUtils.hxx>
#include <chrono>

using namespace std::chrono_literals;

using namespace model;


std::set<int64_t> PostgresDatabaseMedicationDispenseTestBase::sTaskPrescriptionIdsToBeDeleted;

void PostgresDatabaseMedicationDispenseTestBase::cleanup()
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

        Task task = createAcceptedTask(kvnrPatient.id(), prescriptionType, GetParam().isPkv);
        auto medicationDispenses = closeTask(task, pharmacy.id(), GetParam().numMedications, whenPrepared);
        ASSERT_FALSE(medicationDispenses.empty());
        const auto& medicationDispense = medicationDispenses[0];

        PrescriptionId prescriptionId = task.prescriptionId();

        PrescriptionId medicationDispenseId = medicationDispense.prescriptionId();
        ASSERT_EQ(medicationDispenseId, prescriptionId);

        auto medicationDispenseKvnr = medicationDispense.kvnr();
        ASSERT_EQ(medicationDispenseKvnr, kvnrPatient);

        auto medicationDispenseTelematicId = medicationDispense.telematikId();
        ASSERT_EQ(medicationDispenseTelematicId, pharmacy);
        ASSERT_EQ(medicationDispense.whenHandedOver().localDay(), Timestamp::now().localDay());
        if (whenPrepared)
        {
            whenPrepared.emplace(model::Timestamp::GermanTimezone, whenPrepared->localDay());
        }
        ASSERT_EQ(medicationDispense.whenPrepared(), whenPrepared);

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

UrlArguments PostgresDatabaseMedicationDispenseTestBase::createSearchArguments(ServerRequest::QueryParametersType&& queryParameters)
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

void PostgresDatabaseMedicationDispenseTestBase::checkMedicationDispensesXmlStrings(
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

void PostgresDatabaseMedicationDispenseTestBase::writeCurrentTestOutputFile(
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

Task PostgresDatabaseMedicationDispenseTestBase::createAcceptedTask(const std::string_view& kvnrPatient,
                                                                    model::PrescriptionType prescriptionType,
                                                                    bool isPkv)
{
    Task task = createTask(prescriptionType);
    task.setKvnr(model::Kvnr{kvnrPatient});
    task.setIsPkv(isPkv);
    activateTask(task);
    acceptTask(task);
    return task;
}

std::vector<MedicationDispense> PostgresDatabaseMedicationDispenseTestBase::closeTask(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    size_t numMedications,
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
    task.updateLastMedicationDispense();

    std::vector<model::MedicationDispense> medicationDispenses;
    for(size_t i = 0; i < numMedications; ++i)
    {
        medicationDispenses.emplace_back(
            createMedicationDispense(task, telematicIdPharmacy, completedTimestamp, medicationWhenPrepared));
        medicationDispenses.back().setId(model::MedicationDispenseId(medicationDispenses.back().prescriptionId(), i));
    }

    ErxReceipt responseReceipt(
        Uuid(task.receiptUuid().value()), linkBase + "/Task/" + prescriptionId.toString() + "/$close/", prescriptionId,
        compositionResource, authorIdentifier, deviceResource, "TestDigest", prescriptionDigestResource);

    task.setOwner(telematicIdPharmacy);
    task.updateLastMedicationDispense();
    database().updateTaskMedicationDispenseReceipt(task, model::MedicationDispenseBundle{"", medicationDispenses, {}},
                                                   responseReceipt, mJwtBuilder.makeJwtApotheke());
    database().commitTransaction();

    return medicationDispenses;
}

MedicationDispense PostgresDatabaseMedicationDispenseTestBase::createMedicationDispense(
    Task& task,
    const std::string_view& telematicIdPharmacy,
    const Timestamp& whenHandedOver,
    const std::optional<Timestamp>& whenPrepared)
{
    static const decltype(ResourceTemplates::MedicationDispenseOptions::whenPrepared) notPrepared = std::monostate{};
    const auto kvnr = task.kvnr().value().id();
    const ResourceTemplates::MedicationDispenseOptions settings{
        .prescriptionId = task.prescriptionId(),
        .kvnr = kvnr,
        .telematikId = telematicIdPharmacy,
        .whenHandedOver = whenHandedOver,
        .whenPrepared = whenPrepared ? *whenPrepared : notPrepared,
    };
    const auto medicationDispenseXmlString = ResourceTemplates::medicationDispenseXml(settings);

    MedicationDispense medicationDispense = MedicationDispense::fromXmlNoValidation(medicationDispenseXmlString);
    return medicationDispense;
}

Task PostgresDatabaseMedicationDispenseTestBase::createTask(PrescriptionType prescriptionType)
{
    const std::string accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32));

    Task task(prescriptionType, accessCode);

    PrescriptionId prescriptionId = database().storeTask(task);
    task.setPrescriptionId(prescriptionId);

    database().commitTransaction();
    sTaskPrescriptionIdsToBeDeleted.insert(prescriptionId.toDatabaseId());

    return task;
}

void PostgresDatabaseMedicationDispenseTestBase::activateTask(Task& task)
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

    database().activateTask(task, healthCareProviderPrescriptionBinary, mJwtBuilder.makeJwtArzt());
    database().commitTransaction();
}

void PostgresDatabaseMedicationDispenseTestBase::acceptTask(model::Task& task)
{
    task.setStatus(model::Task::Status::inprogress);
    const SafeString secret = SecureRandomGenerator::generate(32);
    task.setSecret(ByteHelper::toHex(secret));
    task.updateLastUpdate();

    database().updateTaskStatusAndSecret(task);
    database().commitTransaction();
}

void PostgresDatabaseMedicationDispenseTestBase::deleteTaskByPrescriptionId(const int64_t prescriptionId)
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

    const auto medicationsAndDispenses =
        database().retrieveAllMedicationDispenses(kvnrPatient, std::optional<UrlArguments>{});
    database().commitTransaction();
    EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
    checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "lt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "lt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 0);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy.id()} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        ServerRequest::QueryParametersType queryParameters{ {"performer", "3-SMC-B-Ungueltig-123455678909876"} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 0);
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
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        const auto& prescriptionId = prescriptionIdsByPatients[kvnrPatient.id()].at(0);
        ServerRequest::QueryParametersType queryParameters{
            {"identifier", std::string(model::resource::naming_system::prescriptionID) + "|" + prescriptionId}};
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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

        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(model::Kvnr{kvnrPatient}, std::optional<UrlArguments>{});
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), prescriptionIds.size() * GetParam().numMedications);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenhandedover", "gt" + whenHandedOver} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = GetParam().numMedications;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        ServerRequest::QueryParametersType queryParameters{ {"whenprepared", "gt" + whenPrepared} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyA.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 2 * GetParam().numMedications;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantC};
        std::string pharmacy = pharmacyA.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = 0;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = pharmacyB.id();
        ServerRequest::QueryParametersType queryParameters{ {"performer", pharmacy} };
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        size_t expectedCount = GetParam().numMedications;
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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
        const auto medicationsAndDispenses =
            database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        const auto& prescriptionIds = prescriptionIdsByPatients[InsurantA];
        for (const auto& prescriptionId : prescriptionIds)
        {
            ServerRequest::QueryParametersType queryParameters{
                {"identifier", std::string(model::resource::naming_system::prescriptionID) + "|" + prescriptionId}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const auto medicationsAndDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
            database().commitTransaction();
            EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
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
            const auto medicationsAndDispenses =
                database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
            database().commitTransaction();
            // ASSERT to avoid endless loop if less than expected count is returned.
            ASSERT_FALSE(std::get<0>(medicationsAndDispenses).medicationDispenses.empty());
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, std::get<0>(medicationsAndDispenses).medicationDispenses);
            if (! std::get<0>(medicationsAndDispenses).medicationDispenses.empty())
            {
                for (size_t idx = 0; idx < std::get<0>(medicationsAndDispenses).medicationDispenses.size() - 1; ++idx)
                {
                    const auto& medicationDispenseThis = std::get<0>(medicationsAndDispenses).medicationDispenses[idx];
                    const auto& medicationDispenseNext = std::get<0>(medicationsAndDispenses).medicationDispenses[idx + 1];
                    ASSERT_GE(medicationDispenseThis.whenHandedOver(), medicationDispenseNext.whenHandedOver());
                }
            }
            receivedCount += std::get<0>(medicationsAndDispenses).medicationDispenses.size();
        }
        ASSERT_EQ(receivedCount, expectedCountAll);
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
        const auto medicationsAndDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
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
        const auto medicationsAndDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
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
        const auto medicationsAndDispenses = database().retrieveAllMedicationDispenses(kvnrPatient, searchArgs);
        database().commitTransaction();
        ASSERT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), expectedCount);
    }

    {
        const auto& prescriptionIds = prescriptionIdsByPatients[InsurantA];
        for (const auto& prescriptionId : prescriptionIds)
        {
            ServerRequest::QueryParametersType queryParameters{
                {"identifier", std::string(model::resource::naming_system::prescriptionID) + "|" + prescriptionId}};
            UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
            const auto medicationsAndDispenses =
                database().retrieveAllMedicationDispenses(model::Kvnr{InsurantA}, searchArgs);
            database().commitTransaction();
            EXPECT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), GetParam().numMedications)
                << model::MedicationDispenseBundle{"", std::get<0>(medicationsAndDispenses).medicationDispenses,
                                                   std::get<0>(medicationsAndDispenses).medications}
                       .serializeToJsonString();
            EXPECT_NO_FATAL_FAILURE(checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings,
                                                                       std::get<0>(medicationsAndDispenses).medicationDispenses));
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
            const auto medicationsAndDispenses = database().retrieveMedicationDispense(kvnrPatient, id);
            database().commitTransaction();
            ASSERT_EQ(medicationsAndDispenses.medicationDispenses.size(), 1);
            EXPECT_EQ(medicationsAndDispenses.medicationDispenses.front().serializeToXmlString(),
                      medicationDispensesInputXmlStrings[std::string(
                          medicationsAndDispenses.medicationDispenses.front().id().toString())]);
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
                const auto medicationsAndDispenses = database().retrieveMedicationDispense(kvnrPatient, id);
                database().commitTransaction();
                ASSERT_EQ(medicationsAndDispenses.medicationDispenses.size(), 1);
                EXPECT_EQ(medicationsAndDispenses.medicationDispenses.front().serializeToXmlString(),
                          medicationDispensesInputXmlStrings[std::string(medicationsAndDispenses.medicationDispenses.front().id().toString())]);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(PostgresDatabaseMedicationDispenseTestInst, PostgresDatabaseMedicationDispenseTest,
                         testing::Values(PostgresDatabaseMedicationDispenseTestParams{1, model::PrescriptionType::apothekenpflichigeArzneimittel, false},
                                         PostgresDatabaseMedicationDispenseTestParams{2, model::PrescriptionType::digitaleGesundheitsanwendungen, false},
                                         PostgresDatabaseMedicationDispenseTestParams{4, model::PrescriptionType::direkteZuweisung, false},
                                         PostgresDatabaseMedicationDispenseTestParams{10, model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, true},
                                         PostgresDatabaseMedicationDispenseTestParams{3, model::PrescriptionType::direkteZuweisungPkv, true},
                                         PostgresDatabaseMedicationDispenseTestParams{1, model::PrescriptionType::tRezept, true},
                                         PostgresDatabaseMedicationDispenseTestParams{1, model::PrescriptionType::tRezept, false}));

std::ostream& operator<<(std::ostream& os, const PostgresDatabaseMedicationDispenseTestParams& params)
{
    os << "numMedications: " << params.numMedications << " type: " << params.type;
    return os;
}

TEST_F(PostgresDatabaseMedicationDispenseTestBase, ERP_30108)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }
    cleanup(); // need empty tables
    {
        auto txn = createTransaction();
        txn.exec("ALTER SEQUENCE erp.task_taskid_seq RESTART WITH 1;");
        txn.exec("ALTER SEQUENCE erp.task_169_taskid_seq RESTART WITH 1;");
        txn.commit();
    }

    Task task1 = createAcceptedTask(InsurantA, PrescriptionType::apothekenpflichigeArzneimittel, false);
    closeTask(task1, "3-SMC-B-Testkarte-883110000120312", 1, std::nullopt);
    Task task2 = createAcceptedTask(InsurantA, PrescriptionType::direkteZuweisung, false);
    closeTask(task2, "3-SMC-B-Testkarte-883110000120312", 1, std::nullopt);

    ASSERT_EQ(task1.prescriptionId().toDatabaseId(), task2.prescriptionId().toDatabaseId());// due to empty tables

    // variant 1
    {
        const model::MedicationDispenseId id(model::PrescriptionId::fromString(task1.prescriptionId().toString()), 0);
        std::optional<MedicationsAndDispenses> medicationsAndDispenses;
        ASSERT_NO_THROW(medicationsAndDispenses = database().retrieveMedicationDispense(model::Kvnr{InsurantA}, id));
        database().commitTransaction();
        ASSERT_EQ(medicationsAndDispenses->medicationDispenses.size(), 1);
    }

    // variant 2:
    {
        ServerRequest::QueryParametersType queryParameters{
            {"identifier",
             std::string(model::resource::naming_system::prescriptionID) + "|" + task1.prescriptionId().toString()}};
        UrlArguments searchArgs = createSearchArguments(std::move(queryParameters));
        std::tuple<model::MedicationsAndDispenses, std::list<model::EuMedicationDispenseInfos>>
            medicationsAndDispenses;
        ASSERT_NO_THROW(medicationsAndDispenses =
                            database().retrieveAllMedicationDispenses(model::Kvnr{InsurantA}, searchArgs));
        database().commitTransaction();
        ASSERT_EQ(std::get<0>(medicationsAndDispenses).medicationDispenses.size(), 1);
    }
}
