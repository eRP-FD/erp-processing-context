/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp10101Test : public ErpWorkflowTest
{
};

TEST_F(Erp10101Test, ReferenceBundle)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_valid_ERP-10101.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "200.000.000.000.002.65", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);
    std::string accessCode{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), accessCode,
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDate("2022-05-31"))));
}
