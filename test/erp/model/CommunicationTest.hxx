/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_COMMUNICATIONTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_COMMUNICATIONTEST_HXX

#include <gtest/gtest.h>

#include "erp/model/Communication.hxx"
#include "erp/model/Task.hxx"

class CommunicationTest : public testing::Test
{
public:
    // Please note that for KVNRs 10 characters should be used.
    static constexpr const char* InsurantA = "X000000012";
    static constexpr const char* InsurantB = "X000000024";
    static constexpr const char* InsurantC = "X000000036";
    static constexpr const char* InsurantX = "X000000048";

    CommunicationTest();

protected:
    std::string mDataPath;
};

#endif
