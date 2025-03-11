/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/model/JsonCanonicalizationTest.hxx"
#include "shared/model/Resource.hxx"
#include "erp/model/Communication.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/String.hxx"


#include <boost/format.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <map>
#include <regex>

#include "test_config.h"

using namespace model;
using namespace model::resource;

namespace rj = rapidjson;

std::string JsonCanonicalizationTest::readInput(const std::string& fileName) const
{
    return FileHelper::readFileAsString(std::string{ TEST_DATA_DIR } + "/fhir/canonicalization/" + fileName + "-In.json");
}

std::string JsonCanonicalizationTest::readExpectedOutput(const std::string& fileName) const
{
    std::string jsonString = FileHelper::readFileAsString(std::string{ TEST_DATA_DIR } + "/fhir/canonicalization/" + fileName + "-Canocalized.json");
    rj::Document document;
    document.Parse(jsonString.data());
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    document.Accept(writer);
    return std::string(buffer.GetString());
}

TEST_F(JsonCanonicalizationTest, Empty)
{
    std::string jsonStringIn;
    EXPECT_THROW(JsonCanonicalizationTestModel::fromJson(jsonStringIn), model::ModelException);
}

TEST_F(JsonCanonicalizationTest, EmptyBody)
{
    std::string jsonStringIn = "{}";
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    EXPECT_THROW((void)model.serializeToCanonicalJsonString(), model::ModelException);
}

TEST_F(JsonCanonicalizationTest, UnknownResource)
{
    std::string jsonStringIn = R"({"resourceType": "Unknown"})";
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    EXPECT_THROW((void)model.serializeToCanonicalJsonString(), model::ModelException);
}

TEST_F(JsonCanonicalizationTest, InvalidResource)
{
    std::string jsonStringIn = R"({"resourceType": "Communication", "contained": [{"amount": {"numerator": { "value": 12 } } } ] })";
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    EXPECT_THROW((void)model.serializeToCanonicalJsonString(), model::ModelException);
}

TEST_F(JsonCanonicalizationTest, EmptyResource)
{
    std::string jsonStringIn = R"({"resourceType": "Communication"})";
    std::string jsonStringOutExpected = R"({"resourceType":"Communication"})";
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, NoDoubleWhitespacesInPropertyValues)
{
    std::string jsonStringIn = readInput("DoubleWhitespacesInPropertyValues");
    std::string jsonStringOutExpected = readExpectedOutput("DoubleWhitespacesInPropertyValues");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, NoDoubleWhitespacesInXhtmlNarrative)
{
    std::string jsonStringIn = readInput("DoubleWhitespacesInXhtmlNarrative");
    std::string jsonStringOutExpected = readExpectedOutput("DoubleWhitespacesInXhtmlNarrative");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, OrderPropertiesAlphabetically)
{
    std::string jsonStringIn = readInput("OrderPropertiesAlphabetically");
    std::string jsonStringOutExpected = readExpectedOutput("OrderPropertiesAlphabetically");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, OmitNarrativeResourceText)
{
    std::string jsonStringIn = readInput("OmitNarrativeResourceText");
    std::string jsonStringOutExpected = readExpectedOutput("OmitNarrativeResourceText");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, OmitResourceMetaElement)
{
    std::string jsonStringIn = readInput("OmitResourceMetaElement");
    std::string jsonStringOutExpected = readExpectedOutput("OmitResourceMetaElement");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, PrimitiveElements)
{
    std::string jsonStringIn = readInput("PrimitiveElements");
    std::string jsonStringOutExpected = readExpectedOutput("PrimitiveElements");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, IntValues)
{
    std::string jsonStringIn = readInput("IntValues");
    std::string jsonStringOutExpected = readExpectedOutput("IntValues");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, Communication1)
{
    std::string jsonStringIn = readInput("Communication1");
    std::string jsonStringOutExpected = readExpectedOutput("Communication1");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, Communication2)
{
    std::string jsonStringIn = readInput("Communication2");
    std::string jsonStringOutExpected = readExpectedOutput("Communication2");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, CommunicationInfoReq)
{
    std::string jsonStringIn = readInput("CommunicationInfoReq");
    std::string jsonStringOutExpected = readExpectedOutput("CommunicationInfoReq");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, Bundle1)
{
    std::string jsonStringIn = readInput("Bundle1");
    std::string jsonStringOutExpected = readExpectedOutput("Bundle1");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, Bundle2)
{
    std::string jsonStringIn = readInput("Bundle2");
    std::string jsonStringOutExpected = readExpectedOutput("Bundle2");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}

TEST_F(JsonCanonicalizationTest, KBVBundle)
{
    std::string jsonStringIn = readInput("KBVBundle");
    std::string jsonStringOutExpected = readExpectedOutput("KBVBundle");
    JsonCanonicalizationTestModel model = JsonCanonicalizationTestModel::fromJson(jsonStringIn);
    std::string jsonStringOut = model.serializeToCanonicalJsonString();
    EXPECT_EQ(jsonStringOut, jsonStringOutExpected);
}
