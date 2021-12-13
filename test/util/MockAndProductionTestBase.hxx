/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKANDPRODUCTIONTESTBASE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKANDPRODUCTIONTESTBASE_HXX

#include <gtest/gtest.h>


/**
 * Test base class that parameterizes tests for execution either with a mock or a production implementation of
 * some service. The idea is that if the production implementation is available then tests are executed for producation
 * and mock implementation and only for mock otherwise.
 *
 * Additionally the parameter is a factory of the actual parameter to avoid having GTEST create static instances
 * of the parameter for each test, regardless of whether the test is actually executed.
 */
template<class ParameterType>
class MockAndProductionTestBase : public testing::TestWithParam<std::function<std::unique_ptr<ParameterType>()>>
{
public:
    std::unique_ptr<ParameterType> parameter;

    virtual void SetUp (void) override
    {
        std::function<std::unique_ptr<ParameterType>()> factory = testing::TestWithParam<std::function<std::unique_ptr<ParameterType>()>>::GetParam();
        parameter = factory();
        if (parameter == nullptr)
            GTEST_SKIP();
#ifndef DEBUG
        GTEST_SKIP_("disabled in Release");
#endif
    }
};

#define InstantiateMockAndProductionTestSuite(BaseName,Fixture,ProductionFactory,MockFactory) \
 INSTANTIATE_TEST_SUITE_P(\
    Simulated##BaseName,\
    Fixture,\
    testing::Values(ProductionFactory),\
    [](auto&){return "simulated";});\
INSTANTIATE_TEST_SUITE_P(\
    Mocked##BaseName,\
    Fixture,\
    testing::Values(MockFactory),\
    [](auto&){return "mocked";})

#endif
