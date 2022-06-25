/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <boost/algorithm/string/replace.hpp>

TEST_F(ErpWorkflowTest, AuthoredOnEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    auto bundleTemplate =
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/kbv_bundle_authoredOn_template.xml");
    patchVersionsInBundle(bundleTemplate);
    boost::replace_all(bundleTemplate, "160.000.000.004.713.80", prescriptionId->toString());

    auto time1 = model::Timestamp::fromXsDateTime("2022-05-26T14:33:00+02:00");
    auto bundle1 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", time1.toXsDate());

    taskActivate(prescriptionId.value(), accessCode, toCadesBesSignature(bundle1, time1), HttpStatus::OK);
}

TEST_F(ErpWorkflowTest, AuthoredOnNotEqualsQesDate)
{
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode));
    ASSERT_TRUE(prescriptionId.has_value());
    ASSERT_FALSE(accessCode.empty());

    auto bundleTemplate =
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/kbv_bundle_authoredOn_template.xml");
    patchVersionsInBundle(bundleTemplate);
    boost::replace_all(bundleTemplate, "160.000.000.004.713.80", prescriptionId->toString());

    auto time1 = model::Timestamp::fromXsDateTime("2022-05-26T14:33:00+02:00");
    auto time2 = model::Timestamp::fromXsDateTime("2022-05-25T14:33:00+02:00");
    auto bundle1 = String::replaceAll(bundleTemplate, "###AUTHORED_ON###", time1.toXsDate());

    taskActivate(prescriptionId.value(), accessCode, toCadesBesSignature(bundle1, time2), HttpStatus::BadRequest,
                 model::OperationOutcome::Issue::Type::invalid,
                 "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein");
}
