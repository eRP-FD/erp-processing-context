/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockRedisStore.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


class ServiceContextTest : public ::testing::Test
{
    virtual void SetUp() override
    {
        tslEnvironmentGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    }

    virtual void TearDown() override
    {
        tslEnvironmentGuard.reset();
    }

    std::unique_ptr<EnvironmentVariableGuard> tslEnvironmentGuard;
};



TEST_F(ServiceContextTest, init)
{
    PcServiceContext serviceContext(
        Configuration::instance(),
        &MockDatabase::createMockDatabase,
        std::make_unique<MockRedisStore>(),
        std::make_unique<HsmPool>(
            std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                             MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
            TeeTokenUpdater::createMockTeeTokenUpdaterFactory()),
        StaticData::getJsonValidator(),
        StaticData::getXmlValidator(),
        StaticData::getInCodeValidator());
    ASSERT_TRUE(serviceContext.databaseFactory());
}


TEST_F(ServiceContextTest, initWithTsl)
{
    PcServiceContext serviceContext(
        Configuration::instance(),
        &MockDatabase::createMockDatabase,
        std::make_unique<MockRedisStore>(),
        std::make_unique<HsmPool>(
            std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                             MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
            TeeTokenUpdater::createMockTeeTokenUpdaterFactory()),
        StaticData::getJsonValidator(),
        StaticData::getXmlValidator(),
        StaticData::getInCodeValidator(),
        TslTestHelper::createTslManager<TslManager>());

    ASSERT_TRUE(serviceContext.databaseFactory());
    ASSERT_NE(nullptr,serviceContext.getTslManager());
    ASSERT_NE(nullptr, serviceContext.getTslManager()->getTslTrustedCertificateStore(TslMode::TSL, std::nullopt).getStore());
    ASSERT_NE(nullptr, serviceContext.getTslManager()->getTslTrustedCertificateStore(TslMode::BNA, std::nullopt).getStore());
}
