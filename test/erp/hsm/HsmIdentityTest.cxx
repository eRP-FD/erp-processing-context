/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmIdentity.hxx"

#include <gtest/gtest.h>


class HsmIdentityTest : public testing::Test
{
};


TEST_F(HsmIdentityTest, getWorkIdentity)
{
    auto workIdentity = HsmIdentity::getWorkIdentity();

    ASSERT_EQ(workIdentity.identity, HsmIdentity::Identity::Work);
}


TEST_F(HsmIdentityTest, getSetupIdentity)
{
    auto setupIdentity = HsmIdentity::getSetupIdentity();

    ASSERT_EQ(setupIdentity.identity, HsmIdentity::Identity::Setup);
}


TEST_F(HsmIdentityTest, getIdentity)
{
    auto workIdentity = HsmIdentity::getIdentity(HsmIdentity::Identity::Work);
    ASSERT_EQ(workIdentity.identity, HsmIdentity::Identity::Work);

    auto setupIdentity = HsmIdentity::getIdentity(HsmIdentity::Identity::Setup);
    ASSERT_EQ(setupIdentity.identity, HsmIdentity::Identity::Setup);
}