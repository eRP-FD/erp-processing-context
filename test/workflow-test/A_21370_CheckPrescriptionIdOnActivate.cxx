/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvBundle.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/ErpRequirements.hxx"
#include "test/util/StaticData.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceTemplates.hxx"

#include "fhirtools/validator/ValidationResult.hxx"

#include <boost/algorithm/string.hpp>



class A_21370_CheckPrescriptionIdOnActivate
    : public ErpWorkflowTestTemplate<::testing::TestWithParam<model::PrescriptionType>>
{
public:
    std::string getBundleWithId(const model::PrescriptionId& id, model::PrescriptionType correctWorkflow)
    {
        return getBundleWithId(id.toString(), correctWorkflow);
    }

    std::string getBundleWithId(const std::string& id, model::PrescriptionType correctWorkflow)
    {
        auto timestamp = model::Timestamp::now();
        auto prescriptionId = model::PrescriptionId::fromStringNoValidation(id);
        std::string bundleXml;
        switch (correctWorkflow)
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
            case model::PrescriptionType::direkteZuweisung:
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            case model::PrescriptionType::direkteZuweisungPkv:
                bundleXml = kbvBundleXml({.prescriptionId = prescriptionId, .authoredOn = timestamp});
                break;
            case model::PrescriptionType::digitaleGesundheitsanwendungen:
                bundleXml = ResourceTemplates::evdgaBundleXml({.prescriptionId = prescriptionId.toString(),
                                                               .timestamp = timestamp.toXsDateTime(),
                                                               .authoredOn = timestamp.toGermanDate()});
                break;
        }
        return toCadesBesSignature(bundleXml, timestamp);
    }
};


TEST_P(A_21370_CheckPrescriptionIdOnActivate, reject)
{
    using namespace std::string_view_literals;
    const auto correctWorkflow = GetParam();
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(correctWorkflow));
    ASSERT_TRUE(task.has_value());
    auto accessCode = std::string{task->accessCode()};
    auto correctId = task->prescriptionId();
    for (auto wrongWorkflow: magic_enum::enum_values<model::PrescriptionType>())
    {
        if (wrongWorkflow == correctWorkflow)
        {
            continue;
        }

        {
            A_21370.test("Abweichender Workflow-Typ (korrigierte prüfsumme)");
            // construct a new PrescriptionId with a different workflow type
            // the checksum will be recalculated, therefore the resulting ID has a different
            // workflow type and a different checksum
            auto wrongId = model::PrescriptionId::fromDatabaseId(wrongWorkflow, correctId.toDatabaseId());
            ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, getBundleWithId(wrongId, correctWorkflow),
                                                HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                                "Flowtype mismatch between Task and QES-Bundle"));
            A_21370.finish();
        }

        {
            A_21370.test("Abweichender Workflow-Typ (falsche prüfsumme)");
            // replace workflow-type without fixing the checksum:
            auto wrongId = correctId.toString();
            wrongId.replace(0, 3, std::to_string(int(wrongWorkflow)));
            // checksum error will already cause BAD REQUEST, therefore we get a diffent operation outcome
            ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, getBundleWithId(wrongId, correctWorkflow),
                                                HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                                "Error while extracting prescriptionId from QES-Bundle"));
            A_21370.finish();
        }
    }
    {
        A_21370.test("Abweichende Id (korrigierte prüfsumme)");
        // construct a new PrescriptionId with a different id-number
        // the checksum will be recalculated, therefore the resulting ID has a different
        // id-number and a different checksum
        auto wrongId = model::PrescriptionId::fromDatabaseId(correctWorkflow, correctId.toDatabaseId() + 1);
        ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, getBundleWithId(wrongId, correctWorkflow),
                                            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                            "PrescriptionId mismatch between Task and QES-Bundle"));
        A_21370.finish();
    }
    {
        A_21370.test("Abweichender Workflow-Typ (falsche prüfsumme)");
        // replace id without fixing the checksum:
        auto wrongId = correctId.toString();
        wrongId.replace(4, 3, (wrongId.substr(4, 3) == "999"sv)?"000"sv:"999"sv);
        // checksum error will already cause BAD REQUEST, therefore we get a diffent operation outcome
        ASSERT_NO_FATAL_FAILURE(taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode, getBundleWithId(wrongId, correctWorkflow),
                                            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                                            "Error while extracting prescriptionId from QES-Bundle"));
        A_21370.finish();
    }
}

INSTANTIATE_TEST_SUITE_P(WorkflowTypes, A_21370_CheckPrescriptionIdOnActivate,
                         testing::ValuesIn(magic_enum::enum_values<model::PrescriptionType>()));
