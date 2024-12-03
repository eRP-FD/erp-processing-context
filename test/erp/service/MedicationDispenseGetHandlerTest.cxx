/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/MedicationDispenseHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/SecureRandomGenerator.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "shared/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/Kvnr.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Uuid.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include "mock/crypto/MockCryptography.hxx"

#include "test/erp/model/CommunicationTest.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test_config.h"

#include <chrono>

using namespace std::chrono_literals;

using namespace model;
using namespace model::resource;

// Tests are parameterized with the number of Medication Dispenses per Task
class MedicationDispenseGetHandlerTest : public ServerTestBase, public testing::WithParamInterface<size_t>
{
public:
    MedicationDispenseGetHandlerTest() :
        ServerTestBase()
    {
    }
protected:
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
    void insertTasks(
        const std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<Timestamp>>>& patientsPharmaciesMedicationWhenPrepared,
        std::set<std::string>& patients,
        std::set<std::string>& pharmacies,
        std::map<std::string, Task>& tasksByPrescriptionIds,
        std::map<std::string, std::vector<MedicationDispense>>& medicationDispensesByPrescriptionIds,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings)
    {
        bool whenPreparedIsDateOnly = ResourceTemplates::Versions::GEM_ERP_current() >= ResourceTemplates::Versions::GEM_ERP_1_4;

        for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
        {
            auto kvnrPatient = std::get<0>(patientAndPharmacy);
            auto pharmacy = std::get<1>(patientAndPharmacy);
            std::optional<Timestamp> whenPrepared = std::get<2>(patientAndPharmacy);

            Task task = createTask();
            activateTask(task, kvnrPatient.id());
            acceptTask(task);

            auto medicationDispenses = closeTask(task, pharmacy.id(), whenPrepared, GetParam());

            PrescriptionId prescriptionId = task.prescriptionId();
            ASSERT_EQ(medicationDispenses.size(), GetParam());

            PrescriptionId medicationDispenseId = medicationDispenses[0].prescriptionId();
            ASSERT_EQ(medicationDispenseId, prescriptionId);

            auto medicationDispenseKvnr = medicationDispenses[0].kvnr();
            ASSERT_EQ(medicationDispenseKvnr, kvnrPatient);

            auto medicationDispenseTelematicId = medicationDispenses[0].telematikId();
            ASSERT_EQ(medicationDispenseTelematicId, pharmacy);
            ASSERT_EQ(medicationDispenses[0].whenHandedOver().localDay(), Timestamp::now().localDay());

            std::optional<Timestamp> medicationDispenseWhenPrepared = medicationDispenses[0].whenPrepared();
            if (whenPreparedIsDateOnly && medicationDispenseWhenPrepared.has_value() && whenPrepared.has_value())
            {
                ASSERT_EQ(medicationDispenseWhenPrepared->localDay(), whenPrepared->localDay());
            }
            else
            {
                ASSERT_EQ(medicationDispenseWhenPrepared, whenPrepared);
            }

            for (size_t i = 0; i  < GetParam(); ++i)
            {
                std::string xmlString = medicationDispenses[i].serializeToXmlString();
                medicationDispensesInputXmlStrings.insert(
                    std::make_pair(model::MedicationDispenseId(prescriptionId, i).toString(), xmlString));
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

    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void sendRequest(
        HttpsClient& client,
        const model::Kvnr& kvnrPatient,
        std::optional<std::string> prescriptionId,
        std::vector<MedicationDispense>& medicationDispensesResponse,
        std::optional<std::vector<std::string>>&& filters = {},
        std::optional<std::vector<std::string>>&& sortings = {},
        HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        std::string path = "/MedicationDispense";

        if (prescriptionId.has_value())
        {
            path += "/" + *prescriptionId;
        }
        if (filters.has_value() && !filters->empty())
        {
            path += "?";
            size_t idx = 0;
            for (const std::string& filter : *filters)
            {
                path += filter;
                if (idx < (filters->size() - 1))
                {
                    path += "&";
                }
                ++idx;
            }
        }
        if (sortings.has_value() && !sortings->empty())
        {
            if (path.find('?') == std::string::npos)
            {
                path += "?";
            }
            else
            {
                path += "&";
            }
            path += "_sort=";
            size_t idx = 0;
            for (const std::string& sort : *sortings)
            {
                path += sort;
                if (idx < (sortings->size() - 1))
                {
                    path += ",";
                }
                ++idx;
            }
        }

        // Please note that each call to "makeJwtVersicherter" creates a jwt with a different signature.
        // The jwt in the header must be the same as in the access token.
        JWT jwt = mJwtBuilder.makeJwtVersicherter(kvnrPatient.id());

        // Create the inner request
        ClientRequest request(createGetHeader(path, jwt), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwt));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        const std::string& responseBody = innerResponse.getBody();
        ASSERT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, expectedHttpStatus));

        if (expectedHttpStatus == HttpStatus::OK)
        {
            if(prescriptionId.has_value())
            {
                if (!responseBody.empty())
                {
                    ASSERT_NO_FATAL_FAILURE(medicationDispensesResponse.push_back(MedicationDispense::fromJsonNoValidation(responseBody)));
                    std::string xmlString = medicationDispensesResponse.back().serializeToXmlString();
                }
            }
            else
            {
                Bundle responseBundle(BundleType::searchset, ::model::FhirResourceBase::NoProfile);
                ASSERT_NO_FATAL_FAILURE(responseBundle = Bundle::fromJsonNoValidation(innerResponse.getBody()));
                for (size_t idxResource = 0; idxResource < responseBundle.getResourceCount(); ++idxResource)
                {
                    ASSERT_NO_FATAL_FAILURE(medicationDispensesResponse.push_back(
                        MedicationDispense::fromJson(responseBundle.getResource(idxResource))));
                    std::string xmlString = medicationDispensesResponse.back().serializeToXmlString();
                }
            }
        }
    }

    void checkMedicationDispensesXmlStrings(
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
};

TEST_P(MedicationDispenseGetHandlerTest, OneTaskGetAllNoFilter)
{
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

    // Create a client
    HttpsClient client = createClient();

    std::vector<MedicationDispense> medicationDispenses;
    sendRequest(client, kvnrPatient, {}, medicationDispenses);
    EXPECT_EQ(medicationDispenses.size(), GetParam());
    checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
}

TEST_P(MedicationDispenseGetHandlerTest, OneTaskGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
{
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

    // Create a client
    auto client = createClient();

    {
        std::vector<std::string> filters = { "whenhandedover=NULL" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=lt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=lt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        std::vector<std::string> filters = { "whenprepared=NULL" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=lt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=lt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        std::vector<std::string> filters = { "performer=" + pharmacy.id() };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::vector<std::string> filters = { "performer==3-SMC-B-Ungueltig-123455678909876" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy.id(),
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

}

TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksGetAllNoFilter)
{
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

    // Create a client
    auto client = createClient();

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses);
        EXPECT_EQ(medicationDispenses.size(), prescriptionIds.size() * GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

// GEMREQ-start A_19406-01#SeveralTasksGetAllSeveralFilters-1
TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
// GEMREQ-end A_19406-01#SeveralTasksGetAllSeveralFilters-1
{
    // GEMREQ-start A_19406-01#SeveralTasksGetAllSeveralFilters-2
    // Insert tasks into database
    //---------------------------

    model::TelematikId pharmacyA{"3-SMC-B-Testkarte-883110000120312"};
    model::TelematikId pharmacyB{"3-SMC-B-Testkarte-883110000120313"};

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
    // GEMREQ-end A_19406-01#SeveralTasksGetAllSeveralFilters-2

    // GEMREQ-start A_19406-01#SeveralTasksGetAllSeveralFilters-3
    // GET Medication Dispenses
    //-------------------------

    auto client = createClient();

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        size_t expectedCount = 3 * GetParam();
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
    // GEMREQ-end A_19406-01#SeveralTasksGetAllSeveralFilters-3

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        size_t expectedCount = GetParam();
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        const auto& pharmacy = pharmacyA;
        std::vector<std::string> filters = { "performer=" + pharmacy.id() };
        size_t expectedCount = 2 * GetParam();
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantC};
        const auto& pharmacy = pharmacyA;
        std::vector<std::string> filters = { "performer=" + pharmacy.id() };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        const auto& pharmacy = pharmacyB;
        std::vector<std::string> filters = { "performer=" + pharmacy.id() };
        size_t expectedCount = GetParam();
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        const auto& pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy.id(),
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = GetParam();
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        model::Kvnr kvnrPatient{InsurantA};
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenprepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenprepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_P(MedicationDispenseGetHandlerTest, ManyTasksGetAllSeveralFilters)//NOLINT(readability-function-cognitive-complexity)
{
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
        medicationDispensesInputXmlStrings);

    // GET Medication Dispenses
    //-------------------------

    // Create a client
    auto client = createClient();

    {
        std::string kvnrPatient = InsurantA;
        const std::string& pharmacy = pharmacyA.id();

        size_t expectedCount = 0;

        for (const auto& patientAndPharmacy : patientsPharmaciesMedicationWhenPrepared)
        {
            if ((std::get<0>(patientAndPharmacy) == kvnrPatient)
                && (std::get<1>(patientAndPharmacy) == pharmacy))
            {
                ++expectedCount;
            }
        }
        expectedCount *= GetParam();

        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        std::vector<std::string> sortings = { "-whenhandedover" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses, std::move(filters), std::move(sortings));
        // ASSERT to avoid endless loop if less than expected count is returned.
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        if (!medicationDispenses.empty())
        {
            for (size_t idx = 0; idx < medicationDispenses.size()-1; ++idx)
            {
                const auto& medicationDispenseThis = medicationDispenses[idx];
                const auto& medicationDispenseNext = medicationDispenses[idx+1];
                ASSERT_GE(medicationDispenseThis.whenHandedOver(), medicationDispenseNext.whenHandedOver());
            }
        }
    }

    for (const std::string& kvnrPatient : patients)
    {
        // whenhandedover is in the future
        const model::TelematikId& pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy.id(),
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses, std::move(filters));
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatient : patients)
    {
        // whenprepared is in the future
        const model::TelematikId& pharmacy = pharmacyA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy.id(),
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses, std::move(filters));
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatient : patients)
    {
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate(Timestamp::GermanTimezone);
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate(Timestamp::GermanTimezone);
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses, std::move(filters));
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_P(MedicationDispenseGetHandlerTest, OneTaskGetById)
{
    // Insert task into database
    //--------------------------

    model::Kvnr kvnrPatient{InsurantA};
    model::TelematikId pharmacy{"3-SMC-B-Testkarte-883110000120312"};

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

    // Create a client
    auto client = createClient();

    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient.id())->second;

    for (const std::string& prescriptionId : prescriptionIds)
    {
        for (size_t i = 0; i < GetParam(); ++i)
        {
            model::MedicationDispenseId medicationId(model::PrescriptionId::fromString(prescriptionId), i);
            std::vector<MedicationDispense> medicationDispenses;
            sendRequest(client, kvnrPatient, medicationId.toString(), medicationDispenses);
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksGetById)
{
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

    // Create a client
    auto client = createClient();

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

        for (const std::string& prescriptionId : prescriptionIds)
        {
            for (size_t i = 0; i < GetParam(); ++i)
            {
                model::MedicationDispenseId medicationId(model::PrescriptionId::fromString(prescriptionId), i);
                std::vector<MedicationDispense> medicationDispenses;
                sendRequest(client, model::Kvnr{kvnrPatient}, medicationId.toString(), medicationDispenses);
                EXPECT_EQ(medicationDispenses.size(), 1);
                checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
            }
        }
    }
}

TEST_P(MedicationDispenseGetHandlerTest, OneTaskFilterById)
{
    using namespace std::string_literals;
    A_22070_02.test("One Task, Multiple Medication Dispenses");
    // ?identifier=https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId|<PrescriptionID>
    model::Kvnr kvnrPatient{InsurantA};
    model::TelematikId pharmacy{"3-SMC-B-Testkarte-883110000120312"};

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

    auto client = createClient();
    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient.id())->second;
    for (const std::string& prescriptionId : prescriptionIds)
    {
        std::vector<model::MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses,
                    {{"identifier="s.append(model::resource::naming_system::prescriptionID) + "|" + prescriptionId}});
        EXPECT_EQ(medicationDispenses.size(), GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksGetByIdUnknownId)
{
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

    // Create a client
    auto client = createClient();

    model::Kvnr kvnrPatient{InsurantA};
    PrescriptionId prescriptionId = PrescriptionId::fromString("160.000.000.004.711.86");
    std::vector<std::string> filters = { "performer=" + pharmacyA.id() };
    std::vector<MedicationDispense> medicationDispenses;
    sendRequest(client, kvnrPatient, prescriptionId.toString(), medicationDispenses,
                std::move(filters), {}, HttpStatus::NotFound);
    EXPECT_EQ(medicationDispenses.size(), 0);
}

TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksFilterById)
{
    using namespace std::string_literals;
    A_22070_02.test("Several Tasks, Multiple Medication Dispenses");
    model::TelematikId pharmacyA{"3-SMC-B-Testkarte-883110000120312"};
    model::TelematikId pharmacyB{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>>
        patientsPharmaciesMedicationWhenPrepared = {{model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantA}, pharmacyB, std::nullopt},
                                                    {model::Kvnr{InsurantB}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantC}, pharmacyB, std::nullopt}};

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies, tasksByPrescriptionIds,
                medicationDispensesByPrescriptionIds, prescriptionIdsByPatients, prescriptionIdsByPharmacies,
                medicationDispensesInputXmlStrings);

    auto client = createClient();

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

        for (const std::string& prescriptionId : prescriptionIds)
        {
            std::vector<MedicationDispense> medicationDispenses;
            sendRequest(
                client, model::Kvnr{kvnrPatient}, {}, medicationDispenses,
                {{"identifier="s.append(model::resource::naming_system::prescriptionID) + "|" + prescriptionId}});
            EXPECT_EQ(medicationDispenses.size(), GetParam());
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksFilterByIdUnknownId)
{
    using namespace std::string_literals;
    A_22070_02.test("One Task, Multiple Medication Dispenses, invalid PrescrptionID");
    model::TelematikId pharmacyA{"3-SMC-B-Testkarte-883110000120312"};
    model::TelematikId pharmacyB{"3-SMC-B-Testkarte-883110000120313"};

    std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>>
        patientsPharmaciesMedicationWhenPrepared = {{model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantA}, pharmacyB, std::nullopt},
                                                    {model::Kvnr{InsurantB}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantA}, pharmacyA, std::nullopt},
                                                    {model::Kvnr{InsurantC}, pharmacyB, std::nullopt}};

    std::set<std::string> patients;
    std::set<std::string> pharmacies;
    std::map<std::string, Task> tasksByPrescriptionIds;
    std::map<std::string, std::vector<MedicationDispense>> medicationDispensesByPrescriptionIds;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPatients;
    std::map<std::string, std::vector<std::string>> prescriptionIdsByPharmacies;
    std::map<std::string, std::string> medicationDispensesInputXmlStrings;

    insertTasks(patientsPharmaciesMedicationWhenPrepared, patients, pharmacies, tasksByPrescriptionIds,
                medicationDispensesByPrescriptionIds, prescriptionIdsByPatients, prescriptionIdsByPharmacies,
                medicationDispensesInputXmlStrings);

    auto client = createClient();

    model::Kvnr kvnrPatient{InsurantA};
    PrescriptionId prescriptionId = PrescriptionId::fromString("160.000.000.004.711.86");

    std::vector<MedicationDispense> medicationDispenses;
    sendRequest(
        client, kvnrPatient, {}, medicationDispenses,
        {{"identifier="s.append(model::resource::naming_system::prescriptionID) + "|" + prescriptionId.toString()}});
    EXPECT_EQ(medicationDispenses.size(), 0);
}



TEST_P(MedicationDispenseGetHandlerTest, SeveralTasksGetAllNoFilter_DefaultSort)
{
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

    // Create a client
    auto client = createClient();

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, model::Kvnr{kvnrPatient}, {}, medicationDispenses);
        EXPECT_EQ(medicationDispenses.size(), prescriptionIds.size() * GetParam());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        A_24438.test("Ensure medication dispense items order by increasing whenHandedOver");
        ASSERT_TRUE(std::ranges::is_sorted(medicationDispenses, std::less{}, &model::MedicationDispense::whenHandedOver));
        A_24438.finish();
    }
}


INSTANTIATE_TEST_SUITE_P(MedicationDispenseGetHandlerTestInst, MedicationDispenseGetHandlerTest, testing::Values(1,3,10));
