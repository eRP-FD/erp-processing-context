/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/MedicationDispenseHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/service/task/CreateTaskHandler.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
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

class MedicationDispenseGetHandlerTest : public ServerTestBase
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
    void insertTasks(
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

            Task task = createTask();
            activateTask(task, std::string(kvnrPatient));
            acceptTask(task);

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

    void sendRequest(
        HttpsClient& client,
        const std::string& kvnrPatient,
        std::optional<std::string> prescriptionId,
        std::vector<MedicationDispense>& medicationDispensesResponse,
        std::size_t& totalSearchMatches,
        std::optional<std::vector<std::string>>&& filters = {},
        std::optional<std::vector<std::string>>&& sortings = {},
        std::optional<std::pair<size_t, size_t>>&& paging = {},
        HttpStatus expectedHttpStatus = HttpStatus::OK)
    {
        totalSearchMatches = 0;
        std::string path = "/MedicationDispense";

        if (prescriptionId.has_value())
        {
            path += "/" + *prescriptionId;
        }
        if (filters.has_value() && !filters->empty())
        {
            path += "?";
            size_t idx = 0;
            for (std::string filter : *filters)
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
            for (std::string sort : *sortings)
            {
                path += sort;
                if (idx < (sortings->size() - 1))
                {
                    path += ",";
                }
                ++idx;
            }
        }
        if (paging.has_value())
        {
            if (path.find('?') == std::string::npos)
            {
                path += "?";
            }
            else
            {
                path += "&";
            }
            path += "_count=" + std::to_string(std::get<0>(*paging));
            path += "&__offset=" + std::to_string(std::get<1>(*paging));
        }

        // Please note that each call to "makeJwtVersicherter" creates a jwt with a different signature.
        // The jwt in the header must be the same as in the access token.
        JWT jwt = mJwtBuilder.makeJwtVersicherter(kvnrPatient);

        // Create the inner request
        ClientRequest request(createGetHeader(path, jwt), "");

        // Send the request.
        auto outerResponse = client.send(encryptRequest(request, jwt));

        // Verify and decrypt the outer response. Also the generic part of the inner response.
        auto innerResponse = verifyOuterResponse(outerResponse);
        std::string responseBody = innerResponse.getBody();
        ASSERT_NO_FATAL_FAILURE(verifyGenericInnerResponse(innerResponse, expectedHttpStatus));

        if (expectedHttpStatus == HttpStatus::OK)
        {
            if(prescriptionId.has_value())
            {
                if (!responseBody.empty())
                {
                    ASSERT_NO_FATAL_FAILURE(medicationDispensesResponse.push_back(
                        MedicationDispense::fromJson(responseBody)));
                    std::string xmlString = medicationDispensesResponse.back().serializeToXmlString();
                }
            }
            else
            {
                Bundle responseBundle(Bundle::Type::searchset);
                ASSERT_NO_FATAL_FAILURE(responseBundle = Bundle::fromJson(innerResponse.getBody()));
                for (size_t idxResource = 0; idxResource < responseBundle.getResourceCount(); ++idxResource)
                {
                    ASSERT_NO_FATAL_FAILURE(medicationDispensesResponse.push_back(
                        MedicationDispense::fromJson(responseBundle.getResource(idxResource))));
                    std::string xmlString = medicationDispensesResponse.back().serializeToXmlString();
                }
                totalSearchMatches = responseBundle.getTotalSearchMatches();
            }
        }
    }

    void checkMedicationDispensesXmlStrings(
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
};

TEST_F(MedicationDispenseGetHandlerTest, OneTaskGetAllNoFilter)
{
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

    // Create a client
    HttpsClient client = createClient();

    std::vector<MedicationDispense> medicationDispenses;
    std::size_t totalSearchMatches = 0;
    sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches);
    EXPECT_EQ(medicationDispenses.size(), 1);
    EXPECT_EQ(totalSearchMatches, 1);
    checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
}

TEST_F(MedicationDispenseGetHandlerTest, OneTaskGetAllSeveralFilters)
{
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

    // Create a client
    auto client = createClient();

    std::size_t totalSearchMatches = 0;
    {
        std::vector<std::string> filters = { "whenhandedover=NULL" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=lt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=lt" + whenHandedOver };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        std::vector<std::string> filters = { "whenprepared=NULL" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=lt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=lt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        std::vector<std::string> filters = { "performer=" + pharmacy };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::vector<std::string> filters = { "performer==3-SMC-B-Ungueltig-123455678909876" };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 0);
    }

    {
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

}

TEST_F(MedicationDispenseGetHandlerTest, SeveralTasksGetAllNoFilter)
{
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

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches);
        EXPECT_EQ(medicationDispenses.size(), prescriptionIds.size());
        EXPECT_EQ(totalSearchMatches, prescriptionIds.size());
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_F(MedicationDispenseGetHandlerTest, SeveralTasksGetAllSeveralFilters)
{
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

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        size_t expectedCount = 3;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        std::vector<std::string> filters = { "whenhandedover=gt" + whenHandedOver };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 1;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = { "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyA;
        std::vector<std::string> filters = { "performer=" + pharmacy };
        size_t expectedCount = 2;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantC;
        std::string pharmacy = pharmacyA;
        std::vector<std::string> filters = { "performer=" + pharmacy };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = pharmacyB;
        std::vector<std::string> filters = { "performer=" + pharmacy };
        size_t expectedCount = 1;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
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
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 1;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }

    {
        std::string kvnrPatient = InsurantA;
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenprepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenprepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        EXPECT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_F(MedicationDispenseGetHandlerTest, ManyTasksGetAllSeveralFilters)
{
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

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

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
            std::vector<std::string> filters = {
                "performer=" + pharmacy,
                "whenhandedover=gt" + whenHandedOver,
                "whenprepared=gt" + whenPrepared };
            std::vector<std::string> sortings = { "-whenhandedover" };
            std::optional<std::pair<size_t, size_t>> paging;
            if (receivedCount > 0)
            {
                paging = {50, receivedCount};
            }
            std::vector<MedicationDispense> medicationDispenses;
            sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters), std::move(sortings), std::move(paging));
            // ASSERT to avoid endless loop if less than expected count is returned.
            ASSERT_EQ(medicationDispenses.size(), expectedCount);
            ASSERT_EQ(totalSearchMatches, expectedCountAll);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
            if (medicationDispenses.size() > 0 )
            {
                for (size_t idx = 0; idx < medicationDispenses.size()-1; ++idx)
                {
                    const auto& medicationDispenseThis = medicationDispenses[idx];
                    const auto& medicationDispenseNext = medicationDispenses[idx+1];
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
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
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
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }

    for (const std::string& kvnrPatient : patients)
    {
        std::string pharmacy = "3-SMC-B-Ungueltig-123455678909876";
        Timestamp timeWhenHandedOverFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenHandedOver = timeWhenHandedOverFilter.toXsDate();
        Timestamp timeWhenPreparedFilter = Timestamp::now() + std::chrono::hours(-24);
        std::string whenPrepared = timeWhenPreparedFilter.toXsDate();
        std::vector<std::string> filters = {
            "performer=" + pharmacy,
            "whenhandedover=gt" + whenHandedOver,
            "whenprepared=gt" + whenPrepared };
        size_t expectedCount = 0;
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, {}, medicationDispenses, totalSearchMatches, std::move(filters));
        ASSERT_EQ(medicationDispenses.size(), expectedCount);
    }
}

TEST_F(MedicationDispenseGetHandlerTest, OneTaskGetById)
{
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

    // Create a client
    auto client = createClient();

    std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;
    std::size_t totalSearchMatches = 0;

    for (const std::string& prescriptionId : prescriptionIds)
    {
        std::vector<MedicationDispense> medicationDispenses;
        sendRequest(client, kvnrPatient, prescriptionId, medicationDispenses, totalSearchMatches);
        EXPECT_EQ(medicationDispenses.size(), 1);
        checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
    }
}

TEST_F(MedicationDispenseGetHandlerTest, SeveralTasksGetById)
{
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

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    for (const std::string& kvnrPatient : patients)
    {
        std::vector<std::string>& prescriptionIds = prescriptionIdsByPatients.find(kvnrPatient)->second;

        for (const std::string& prescriptionId : prescriptionIds)
        {
            std::vector<MedicationDispense> medicationDispenses;
            sendRequest(client, kvnrPatient, prescriptionId, medicationDispenses, totalSearchMatches);
            EXPECT_EQ(medicationDispenses.size(), 1);
            checkMedicationDispensesXmlStrings(medicationDispensesInputXmlStrings, medicationDispenses);
        }
    }
}

TEST_F(MedicationDispenseGetHandlerTest, SeveralTasksGetByIdUnknownId)
{
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

    // Create a client
    auto client = createClient();
    std::size_t totalSearchMatches = 0;

    std::string kvnrPatient = InsurantA;
    PrescriptionId prescriptionId = PrescriptionId::fromString("160.000.000.004.711.86");
    std::vector<std::string> filters = { "performer=" + pharmacyA };
    std::vector<MedicationDispense> medicationDispenses;
    sendRequest(client, kvnrPatient, prescriptionId.toString(), medicationDispenses, totalSearchMatches,
                std::move(filters), {}, {}, HttpStatus::NotFound);
    EXPECT_EQ(medicationDispenses.size(), 0);
    EXPECT_EQ(totalSearchMatches, 0);
}
