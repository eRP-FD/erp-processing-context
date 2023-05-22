/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp9770Test : public ErpWorkflowTest
{
};

TEST_F(Erp9770Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    if (!Configuration::instance().featurePkvEnabled())
    {
        GTEST_SKIP();
    }
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 3954242);
    send(RequestArguments(HttpMethod::DELETE, "/ChargeItem/" + prescriptionId.toString(), "{}").withJwt(jwtApotheke()));
}
