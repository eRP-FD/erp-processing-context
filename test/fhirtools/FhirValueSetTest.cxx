/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"

#include <gtest/gtest.h>


using namespace fhirtools;

class FhirValueSetTest : public ::testing::Test
{
protected:
};

TEST_F(FhirValueSetTest, OperatorSpaceship)
{
    FhirValueSet::Code code1{.code = "Code", .caseSensitive = true, .codeSystem = "CodeSystem"};
    FhirValueSet::Code code2{.code = "Code", .caseSensitive = false, .codeSystem = ""};
    const auto result = code1 <=> code2;
    EXPECT_EQ(result, std::strong_ordering::equal);
    EXPECT_TRUE(code1 == code2);
    EXPECT_TRUE(code1 <= code2);
    EXPECT_TRUE(code1 >= code2);
    EXPECT_FALSE(code1 < code2);
    EXPECT_FALSE(code1 > code2);
}

TEST_F(FhirValueSetTest, OperatorSpaceshipCaseInSensitive)
{
    FhirValueSet::Code code1{.code = "[Code]", .caseSensitive = false, .codeSystem = "CodeSystem"};
    FhirValueSet::Code code2{.code = "[CoDE]", .caseSensitive = false, .codeSystem = "CodeSystem"};
    const auto result = code1 <=> code2;
    EXPECT_EQ(result, std::strong_ordering::equal);
    EXPECT_TRUE(code1 == code2);
    EXPECT_TRUE(code1 <= code2);
    EXPECT_TRUE(code1 >= code2);
    EXPECT_FALSE(code1 < code2);
    EXPECT_FALSE(code1 > code2);
}


TEST(FhirValueSetTestContains, run)
{
    using namespace fhirtools::version_literal;
    FhirStructureRepositoryBackend repo;
    FhirValueSet::Builder builder;
    builder.url("test");
    builder.name("test");
    builder.version("test"_ver);
    builder.newExpand();
    builder.expandSystem(DefinitionKey{"CodeSystem"});
    builder.expandCode("Value");
    builder.initGroup(FhirResourceGroupConst{"test"}, {});
    auto valueSet = builder.getAndReset();
    valueSet.finalize(&repo);
    ASSERT_TRUE(valueSet.finalized());
    EXPECT_TRUE(valueSet.containsCode("Value"));
    EXPECT_TRUE(valueSet.containsCode("Value", ""));
    EXPECT_TRUE(valueSet.containsCode("Value", "CodeSystem"));
    EXPECT_FALSE(valueSet.containsCode("Value", "WrongCodeSystem"));
    EXPECT_FALSE(valueSet.containsCode("WrongValue", "CodeSystem"));
    EXPECT_FALSE(valueSet.containsCode("WrongValue"));
}
