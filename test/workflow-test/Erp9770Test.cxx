/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

class Erp9770Test : public ErpWorkflowTest
{
};

TEST_F(Erp9770Test, run)//NOLINT(readability-function-cognitive-complexity)
{
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 3954242);
    send(RequestArguments(HttpMethod::DELETE, "/ChargeItem/" + prescriptionId.toString(), "{}").withJwt(jwtApotheke()));
}
