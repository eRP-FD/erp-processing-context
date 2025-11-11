/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASEMEDICATIONDISPENSETEST_HXX

#include <ostream>
#include "test/erp/database/PostgresDatabaseTestFixture.hxx"
#include "shared/server/request/ServerRequest.hxx"

struct PostgresDatabaseMedicationDispenseTestParams
{
    size_t numMedications = 1;
    model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel;
    friend std::ostream& operator<<(std::ostream& os, const PostgresDatabaseMedicationDispenseTestParams& params);
};

class PostgresDatabaseMedicationDispenseTestBase : public PostgresDatabaseTest
{
public:
    void cleanup() override;
protected:
    UrlArguments createSearchArguments(ServerRequest::QueryParametersType&& queryParameters);
    void checkMedicationDispensesXmlStrings(
        std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
        const std::vector<model::MedicationDispense>& medicationDispenses);
    void writeCurrentTestOutputFile(
        const std::string& testOutput,
        const std::string& fileExtension,
        const std::string& marker = std::string()) const;
    bool writeTestOutputFileEnabled = false;

    model::Task createAcceptedTask(const std::string_view& kvnrPatient, model::PrescriptionType prescriptionType);
    std::vector<model::MedicationDispense> closeTask(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        size_t numMedications,
        const std::optional<model::Timestamp>& medicationWhenPrepared = std::nullopt);
    model::MedicationDispense createMedicationDispense(
        model::Task& task,
        const std::string_view& telematicIdPharmacy,
        const model::Timestamp& whenHandedOver,
        const std::optional<model::Timestamp>& whenPrepared = std::nullopt);
    model::Task createTask(model::PrescriptionType prescriptionType);
    void activateTask(model::Task& task);
    void acceptTask(model::Task& task);
    void deleteTaskByPrescriptionId(const int64_t prescriptionId);

    // Storing the ids retrieved from the insert commands ensures that each test
    // starts with a tidy database even if a test failed and was not executed completely.
    // But this will not cleanup the database if the complete test has been interrupted
    // during a debug session.
    static std::set<int64_t> sTaskPrescriptionIdsToBeDeleted;
};

class PostgresDatabaseMedicationDispenseTest
    : public PostgresDatabaseMedicationDispenseTestBase,
      public testing::WithParamInterface<PostgresDatabaseMedicationDispenseTestParams>
{
protected:
    void
    insertTasks(const std::vector<std::tuple<model::Kvnr, model::TelematikId, std::optional<model::Timestamp>>>&
                    patientsAndPharmacies,
                std::set<std::string>& patients, std::set<std::string>& pharmacies,
                std::map<std::string, model::Task>& tasksByPrescriptionIds,
                std::map<std::string, std::vector<model::MedicationDispense>>& medicationDispensesByPrescriptionIds,
                std::map<std::string, std::vector<std::string>>& prescriptionIdsByPatients,
                std::map<std::string, std::vector<std::string>>& prescriptionIdsByPharmacies,
                std::map<std::string, std::string>& medicationDispensesInputXmlStrings,
                model::PrescriptionType prescriptionType = GetParam().type);
};

#endif
