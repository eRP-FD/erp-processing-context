/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONCANONICALIZATIONTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONCANONICALIZATIONTEST_HXX

#include <gtest/gtest.h>

#include "test/erp/model/JsonCanonicalizationTestModel.hxx"

class JsonCanonicalizationTest : public testing::Test
{
public:
    std::string readInput(const std::string& fileName) const;
    std::string readExpectedOutput(const std::string& fileName) const;
};

#endif
