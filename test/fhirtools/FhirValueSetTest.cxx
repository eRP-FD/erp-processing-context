/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


using namespace fhirtools;

class FhirValueSetTest : public ::testing::Test
{
protected:
};

TEST_F(FhirValueSetTest, OperatorSpaceship)
{
    FhirValueSetCodes::Code code1{.code = "Code", .caseSensitive = true, .codeSystem = "CodeSystem"};
    FhirValueSetCodes::Code code2{.code = "Code", .caseSensitive = false, .codeSystem = ""};
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
    FhirValueSetCodes::Code code1{.code = "[Code]", .caseSensitive = false, .codeSystem = "CodeSystem"};
    FhirValueSetCodes::Code code2{.code = "[CoDE]", .caseSensitive = false, .codeSystem = "CodeSystem"};
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
    FhirResourceGroupConst resolver{"test"};
    builder.url("test");
    builder.name("test");
    builder.version("test"_ver);
    builder.newExpand();
    builder.expandSystem(DefinitionKey{"CodeSystem"});
    builder.expandCode("Value");
    builder.initGroup(resolver);
    auto valueSet = builder.getAndReset();
    auto view = fhirtools::FhirResourceViewGroupSet::create("view", resolver.group(), &repo);
    auto velueSetCodes = FhirValueSetCodes::create(view.get(), &valueSet);
    EXPECT_TRUE(velueSetCodes->containsCode("Value"));
    EXPECT_TRUE(velueSetCodes->containsCode("Value", ""));
    EXPECT_TRUE(velueSetCodes->containsCode("Value", "CodeSystem"));
    EXPECT_FALSE(velueSetCodes->containsCode("Value", "WrongCodeSystem"));
    EXPECT_FALSE(velueSetCodes->containsCode("WrongValue", "CodeSystem"));
    EXPECT_FALSE(velueSetCodes->containsCode("WrongValue"));
}

TEST(FhirValueSetTestContains, ExcludeFilter)
{
    using namespace fhirtools::version_literal;
    const FhirResourceGroupConst resolver{"test"};
    FhirStructureRepositoryBackend repo;
    ASSERT_NO_THROW(repo.load(
        {
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/test_code_system.xml"),
        },
        resolver));

    FhirValueSet::Builder builder;
    builder.url("test");
    builder.name("test");
    builder.version("test"_ver);
    builder.include().includeCodeSystem(DefinitionKey{"http://erp.test/TestCodeSystem2|3"});
    builder.exclude()
        .excludeCodeSystem(DefinitionKey{"http://erp.test/TestCodeSystem2|3"})
        .excludeFilter()
        .excludeFilterOp("=")
        .excludeFilterProperty("deprecated")
        .excludeFilterValue("2.9");
    builder.initGroup(resolver);
    auto valueSet = builder.getAndReset();
    auto view = fhirtools::FhirResourceViewGroupSet::create("view", resolver.group(), &repo);
    auto valueSetCodes = FhirValueSetCodes::create(view.get(), &valueSet);

    ASSERT_TRUE(valueSetCodes->containsCode("Test1"));
    ASSERT_FALSE(valueSetCodes->containsCode("Test2"));
}
