/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Environment.hxx"
#include "workflow-test/HttpsTestClient.hxx"

#include <gtest/gtest.h>


int main (int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "1");
    TestClient::setFactory(&HttpsTestClient::factory);

    GLogConfiguration::init_logging(argv[0]);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
