#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONMAINTAINNUMBERPRECISIONTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONMAINTAINNUMBERPRECISIONTEST_HXX

#include <gtest/gtest.h>

#include "test/erp/model/JsonMaintainNumberPrecisionTestModel.hxx"

#include <unordered_map>

class JsonMaintainNumberPrecisionTest : public testing::Test
{
public:
    static std::string createJsonStringCommunicationInfoReq();
};

#endif
