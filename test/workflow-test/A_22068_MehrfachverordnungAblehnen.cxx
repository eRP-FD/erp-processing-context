/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H
#define ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H

#include "erp/model/OperationOutcome.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "test/util/ResourceManager.hxx"

class A_22068_MehrfachverordnungAblehnen : public ErpWorkflowTest
{
public:
};


TEST_F(A_22068_MehrfachverordnungAblehnen, run)
{
    auto& resourceManager = ResourceManager::instance();
    auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung.xml");
    bundle = patchVersionsInBundle(bundle);
    std::string kvnr;
    ASSERT_NO_FATAL_FAILURE(generateNewRandomKVNR(kvnr));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto accessCode = std::string{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), accessCode, toCadesBesSignature(bundle), HttpStatus::BadRequest,
                     model::OperationOutcome::Issue::Type::invalid, "Mehrfachverordnung nicht zulässig")
    );
}


#endif// ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H
