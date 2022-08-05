/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX

#include <ostream>
#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "erp/server/request/ServerRequest.hxx"

struct PostgresDatabaseMedicationDispenseTestParams
{
    size_t numMedications = 1;
    model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel;
    friend std::ostream& operator<<(std::ostream& os, const PostgresDatabaseMedicationDispenseTestParams& params);
};

class PostgresDatabaseMedicationDispenseTest : public PostgresDatabaseTest, public testing::WithParamInterface<PostgresDatabaseMedicationDispenseTestParams>
{
public:
    PostgresDatabaseMedicationDispenseTest();
    void cleanup() override;
protected:
    void insertTasks(
        const std::vector<std::tuple<std::string, std::string, std::optional<fhirtools::Timestamp>>>& patientsAndPharmacies,
        std::set<std::string>& patients,
        std::set<std::string>& pharmacies,
        std::map<std::string, model::Task>& tasksByPrescriptionIds,
        std::map<std::string, std::vector<model::MedicationDispense>>& medicationDispensesByPrescriptionIds,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
        model::PrescriptionType prescriptionType = GetParam().type);
    UrlArguments createSearchArguments(ServerRequest::QueryParametersType&& queryParameters);
    void checkMedicationDispensesXmlStrings(
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
        const std::vector<model::MedicationDispense>& medicationDispenses);
    void writeCurrentTestOutputFile(
        const std::string& testOutput,
        const std::string& fileExtension,
        const std::string& marker = std::string()) const;
    bool writeTestOutputFileEnabled = false;
private:
    model::Task createAcceptedTask(const std::string_view& kvnrPatient, model::PrescriptionType prescriptionType = GetParam().type);
    std::vector<model::MedicationDispense> closeTask(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const std::optional<fhirtools::Timestamp>& medicationWhenPrepared = std::nullopt);
    model::MedicationDispense createMedicationDispense(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const fhirtools::Timestamp& whenHandedOver,
        const std::optional<fhirtools::Timestamp>& whenPrepared = std::nullopt);
    model::Task createTask(model::PrescriptionType prescriptionType = GetParam().type);
    void activateTask(model::Task& task);
    void acceptTask(model::Task& task);
    void deleteTaskByPrescriptionId(const int64_t prescriptionId);

    // Storing the ids retrieved from the insert commands ensures that each test
    // starts with a tidy database even if a test failed and was not executed completely.
    // But this will not cleanup the database if the complete test has been interrupted
    // during a debug session.
    static std::set<int64_t> sTaskPrescriptionIdsToBeDeleted;
};

#endif
