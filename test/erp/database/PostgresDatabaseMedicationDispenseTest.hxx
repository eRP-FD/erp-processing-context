#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX

#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "erp/server/request/ServerRequest.hxx"


class PostgresDatabaseMedicationDispenseTest : public PostgresDatabaseTest
{
public:
    PostgresDatabaseMedicationDispenseTest();
    void cleanup() override;
protected:
    void insertTasks(
        const std::vector<std::tuple<std::string, std::string, std::optional<model::Timestamp>>>& patientsAndPharmacies,
        std::set<std::string>& patients,
        std::set<std::string>& pharmacies,
        std::map<std::string, model::Task>& tasksByPrescriptionIds,
        std::map<std::string, model::MedicationDispense>& medicationDispensesByPrescriptionIds,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
        std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings);
    UrlArguments createSearchArguments(ServerRequest::QueryParametersType&& queryParameters);
    void checkMedicationDispensesXmlStrings(
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
        const std::vector<model::MedicationDispense>& medicationDispenses);
    void writeCurrentTestOutputFile(
        const std::string& testOutput,
        const std::string& fileExtension,
        const std::string& marker = std::string());
    bool writeTestOutputFileEnabled = false;
private:
    model::Task createAcceptedTask(const std::string_view& kvnrPatient);
    model::MedicationDispense closeTask(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const std::optional<model::Timestamp>& medicationWhenPrepared = std::nullopt);
    model::MedicationDispense createMedicationDispense(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const model::Timestamp& whenHandedOver,
        const std::optional<model::Timestamp>& whenPrepared = std::nullopt);
    model::Task createTask();
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
