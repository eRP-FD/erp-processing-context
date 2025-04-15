/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKANDPRODUCTIONTESTBASE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKANDPRODUCTIONTESTBASE_HXX

#include "shared/tpm/Tpm.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>
#include <optional>


/**
 * Test base class that parametrizes tests for execution either with a mock or a production implementation of
 * some service. The idea is that if the production implementation is available then tests are executed for production
 * and mock implementation and only for mock otherwise.
 *
 */
class MockAndProductionTestBase : public testing::TestWithParam<MockBlobCache::MockTarget>
{
public:
    Tpm::Factory tpmFactory;

    PcServiceContext* mContext{nullptr};
    std::optional<PcServiceContext> mSimulatedContext;
    std::optional<PcServiceContext> mMockedContext;

    void SetUp() override;
};

#endif
