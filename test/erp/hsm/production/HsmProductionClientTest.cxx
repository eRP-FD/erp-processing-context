/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/production/HsmProductionClient.hxx"

#include "shared/hsm/production/HsmRawSession.hxx"
#include "shared/hsm/HsmIdentity.hxx"
#include "shared/util/Configuration.hxx"

#include <gtest/gtest.h>
#include "test/util/EnvironmentVariableGuard.hxx"

#include "test_config.h"

class HsmProductionClientTest : public testing::Test
{
public:
    void skipIfUnsupported (void)
    {
#ifdef __APPLE__
        GTEST_SKIP():
#endif
        auto device = Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE);
        if (device.empty())
            GTEST_SKIP();
        else
            TVLOG(1) << "will connect to HSM with " << device;
    }
};


TEST_F(HsmProductionClientTest, hsmErrorCodeString)
{
    const auto ok = HsmProductionClient::hsmErrorCodeString(ERP_ERR_NOERROR);

    ASSERT_EQ(ok, "ERP_ERR_NOERROR");
}


TEST_F(HsmProductionClientTest, hsmErrorIndexString)
{
    {
        const auto indexString = HsmProductionClient::hsmErrorIndexString(ERP_ERR_NOERROR);
        ASSERT_EQ(indexString, "00");
    }

    {
        const auto indexString = HsmProductionClient::hsmErrorIndexString(ERP_ERR_PERMISSION_DENIED | 0x00001f00);
        ASSERT_EQ(indexString, "1f");
    }
}


TEST_F(HsmProductionClientTest, hsmErrorDetails)
{
    {
        const auto details = HsmProductionClient::hsmErrorDetails(ERP_ERR_NOERROR);
        ASSERT_EQ(details, "0 ERP_ERR_NOERROR without index");
    }

    {
        const auto details = HsmProductionClient::hsmErrorDetails(ERP_ERR_PERMISSION_DENIED | 0x0000ff00);
        ASSERT_EQ(details, "b101ff01 ERP_ERR_PERMISSION_DENIED index ff");
    }
}


TEST_F(HsmProductionClientTest, connectAsWorkUser)
{
    if (Configuration::instance().getOptionalStringValue(ConfigurationKey::HSM_DEVICE, "").empty())
        GTEST_SKIP();

    ASSERT_NO_THROW(
        HsmProductionClient::connect(HsmIdentity::getWorkIdentity()));
}


TEST_F(HsmProductionClientTest, connectAsSetupUser)
{
    if (Configuration::instance().getOptionalStringValue(ConfigurationKey::HSM_DEVICE, "").empty())
        GTEST_SKIP();

    ASSERT_NO_THROW(
        HsmProductionClient::connect(HsmIdentity::getSetupIdentity()));
}


TEST_F(HsmProductionClientTest, connectAsWorkUserWithKeyspec)
{
    if (Configuration::instance().getOptionalStringValue(ConfigurationKey::HSM_DEVICE, "").empty())
        GTEST_SKIP();

    // Temporarily setup ERP_WORK as ERP_KWRK together with a keyspec file and password.
    EnvironmentVariableGuard workUserName ("ERP_HSM_WORK_USERNAME", "ERP_KWRK");
    EnvironmentVariableGuard workKeySpec ("ERP_HSM_WORK_KEYSPEC", std::string(TEST_DATA_DIR) + "/enrolment/ERP_KWRK_keyfile.key");
    EnvironmentVariableGuard workPassword ("ERP_HSM_WORK_PASSWORD", "RUTU");

    // getWorkIdentity will pick up the new values of all three variables and use the keyspec file to login to the HSM.
    ASSERT_NO_THROW(
        HsmProductionClient::connect(HsmIdentity::getWorkIdentity()));
}
