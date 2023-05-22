/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Environment.hxx"
#include "erp/util/TLog.hxx"

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    GLogConfiguration::init_logging(argv[0]);
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
