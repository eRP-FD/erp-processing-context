/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class RegressionTest : public ErpWorkflowTest
{
};

TEST_F(RegressionTest, Erp10674)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_invalid_ERP-10674.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.008.870.312.04", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);
    std::string accessCode{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), accessCode,
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDate("2022-07-29")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10669)
{
    std::string kbv_bundle_xml = ResourceManager::instance().getStringResource(
        "test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_22635.xml");
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "####START####", "2022-07-29T00:05:57+02:00");
    kbv_bundle_xml =
        String::replaceAll(kbv_bundle_xml, "<authoredOn value=\"2020-02-03\"/>", "<authoredOn value=\"2022-07-29\"/>");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.004.713.80", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivate(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00"))));
    ASSERT_TRUE(taskActivateResult);
    EXPECT_EQ(taskActivateResult->expiryDate().toGermanDate(), "2023-07-29");
    EXPECT_EQ(taskActivateResult->acceptDate().toGermanDate(), "2023-07-29");
    auto bundle = taskGetId(taskActivateResult->prescriptionId(), std::string{taskActivateResult->kvnr().value()});
    ASSERT_TRUE(bundle);
    std::optional<model::Task> getTaskResult;
    getTaskFromBundle(getTaskResult, *bundle);
    ASSERT_TRUE(getTaskResult);
    EXPECT_EQ(getTaskResult->expiryDate().toGermanDate(), "2023-07-29");
    EXPECT_EQ(getTaskResult->acceptDate().toGermanDate(), "2023-07-29");
}

TEST_F(RegressionTest, Erp10892_1)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-10892/meta_profile_array_size.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.012.588.26", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivate(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10892_2)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-10892/negative_timestamp.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.000.012.588.26", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivate(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}
