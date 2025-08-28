/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockFhirResourceView.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/VersionMappingView.hxx"

#include <gtest/gtest.h>
#include <magic_enum/magic_enum.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stream.h>

using namespace fhirtools::version_literal;

class VersionMappingViewTest : public ::testing::Test
{
public:
    template<typename T>
    std::unique_ptr<T> make(const fhirtools::DefinitionKey& key)
    {
        typename T::Builder builder;
        builder.url(key.url);
        builder.version(*key.version);
        builder.initGroup(resolver);
        if constexpr (std::is_same_v<T, fhirtools::FhirStructureDefinition>)
        {
            builder.repositoryBackend(&repo);
            builder.typeId("string");
        }
        if constexpr (std::is_same_v<std::unique_ptr<T>, decltype(builder.getAndReset())>)
        {
            return builder.getAndReset();
        }
        else
        {
            return std::make_unique<T>(builder.getAndReset());
        }
    }

    void SetUp() override
    {
        using DefinitionKey = fhirtools::DefinitionKey;

        structure = make<fhirtools::FhirStructureDefinition>(structKey);
        codeSystem = make<fhirtools::FhirCodeSystem>(codeSysKey);
        valueSet = make<fhirtools::FhirValueSet>(valueSetKey);

        using ::testing::Eq;
        using ::testing::Return;
        ON_CALL(*baseView, findStructure(Eq(structKey))).WillByDefault(Return(structure.get()));
        ON_CALL(*baseView, findCodeSystem(Eq(codeSysKey))).WillByDefault(Return(codeSystem.get()));
        ON_CALL(*baseView, findValueSet(Eq(valueSetKey))).WillByDefault(Return(valueSet.get()));
        ON_CALL(*baseView, findStructure(Eq(DefinitionKey{structKey.url, std::nullopt})))
            .WillByDefault(Return(structure.get()));
        ON_CALL(*baseView, findCodeSystem(Eq(DefinitionKey{codeSysKey.url, std::nullopt})))
            .WillByDefault(Return(codeSystem.get()));
        ON_CALL(*baseView, findValueSet(Eq(DefinitionKey{valueSetKey.url, std::nullopt})))
            .WillByDefault(Return(valueSet.get()));
    }

    const fhirtools::DefinitionKey structKey{"http://erp.test/TestStructure", "0.8.15"_ver};
    const fhirtools::DefinitionKey codeSysKey{"http://erp.test/CodeSystem", "0.8.15"_ver};
    const fhirtools::DefinitionKey valueSetKey{"http://erp.test/ValueSet", "0.8.15"_ver};

    fhirtools::FhirStructureRepositoryBackend repo;
    fhirtools::FhirResourceGroupConst resolver{"test"};
    std::unique_ptr<fhirtools::FhirStructureDefinition> structure;
    std::unique_ptr<fhirtools::FhirCodeSystem> codeSystem;
    std::unique_ptr<fhirtools::FhirValueSet> valueSet;

    std::shared_ptr<fhirtools::test::MockFhirResourceView> baseView = fhirtools::test::MockFhirResourceView::createNice();
};

TEST_F(VersionMappingViewTest, map)
{
    auto mapping = fhirtools::VersionMappingView::create(
        "mapping", fhirtools::VersionMapper{{.map = {{R"(http://erp\.test/.*)", R"(0\.8)", "0.8.15"_ver, "0.8"_ver}}}},
        baseView);

    // positive tests:

    // search with full key
    EXPECT_EQ(mapping->findStructure(structKey), structure.get());
    // search with mapped version
    EXPECT_EQ(mapping->findStructure({structKey.url, "0.8"_ver}), structure.get());
    // search without version
    EXPECT_EQ(mapping->findStructure({structKey.url, std::nullopt}), structure.get());

    // search with full key
    EXPECT_EQ(mapping->findCodeSystem(codeSysKey), codeSystem.get());
    // search with mapped version
    EXPECT_EQ(mapping->findCodeSystem({codeSysKey.url, "0.8"_ver}), codeSystem.get());
    // search without version
    EXPECT_EQ(mapping->findCodeSystem({codeSysKey.url, std::nullopt}), codeSystem.get());

    // search with full key
    EXPECT_EQ(mapping->findValueSet(valueSetKey), valueSet.get());
    // search with mapped version
    EXPECT_EQ(mapping->findValueSet({valueSetKey.url, "0.8"_ver}), valueSet.get());
    // search without version
    EXPECT_EQ(mapping->findValueSet({valueSetKey.url, std::nullopt}), valueSet.get());


    // negative tests:

    // search structure with non-matching version
    EXPECT_EQ(mapping->findStructure({structKey.url, "0.9"_ver}), nullptr);
    // use CodeSystem-Key for Structure
    EXPECT_EQ(mapping->findStructure(codeSysKey), nullptr);
    // use ValueSet-Key for Structure
    EXPECT_EQ(mapping->findStructure(valueSetKey), nullptr);
    // use CodeSystem-Key with short Version for Structure
    EXPECT_EQ(mapping->findStructure({codeSysKey.url, "0.8"_ver}), nullptr);
    // use ValueSet-Key with short Version for Structure
    EXPECT_EQ(mapping->findStructure({valueSetKey.url, "0.8"_ver}), nullptr);
    // use CodeSystem-Key without Version for Structure
    EXPECT_EQ(mapping->findStructure({codeSysKey.url, std::nullopt}), nullptr);
    // use ValueSet-Key without Version for Structure
    EXPECT_EQ(mapping->findStructure({valueSetKey.url, std::nullopt}), nullptr);

    // search value set with non-matching version
    EXPECT_EQ(mapping->findValueSet({valueSetKey.url, "0.9"_ver}), nullptr);
    // use CodeSystem-Key for ValueSet
    EXPECT_EQ(mapping->findValueSet(codeSysKey), nullptr);
    // use Structure-Key for ValueSet
    EXPECT_EQ(mapping->findValueSet(structKey), nullptr);
    // use CodeSystem-Key with short Version for ValueSet
    EXPECT_EQ(mapping->findValueSet({codeSysKey.url, "0.8"_ver}), nullptr);
    // use Structuret-Key with short Version for ValueSet
    EXPECT_EQ(mapping->findValueSet({structKey.url, "0.8"_ver}), nullptr);
    // use CodeSystem-Key without Version for ValueSet
    EXPECT_EQ(mapping->findValueSet({codeSysKey.url, std::nullopt}), nullptr);
    // use Structure-Key without Version for ValueSet
    EXPECT_EQ(mapping->findValueSet({structKey.url, std::nullopt}), nullptr);

    // search value set with non-matching version
    EXPECT_EQ(mapping->findCodeSystem({codeSysKey.url, "0.9"_ver}), nullptr);
    // use ValueSet-Key for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem(valueSetKey), nullptr);
    // use Structure-Key for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem(structKey), nullptr);
    // use ValueSet-Key with short Version for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem({valueSetKey.url, "0.8"_ver}), nullptr);
    // use Structuret-Key with short Version for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem({structKey.url, "0.8"_ver}), nullptr);
    // use ValueSet-Key without Version for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem({valueSetKey.url, std::nullopt}), nullptr);
    // use Structure-Key without Version for CodeSystem
    EXPECT_EQ(mapping->findCodeSystem({structKey.url, std::nullopt}), nullptr);
}

TEST(VersionMapper, configFromJson)
{
    using Config = fhirtools::VersionMapper::Config;
    using namespace fhirtools::version_literal;
    static constexpr char configJson[] =//
        R"(
[
    {"urlRegex": "urlRegex1", "versionRegex": "versionRegex1", "realVersion": "realVersion1", "renderVersion": "renderVersion1" },
    {"urlRegex": "urlRegex2", "versionRegex": "versionRegex2", "realVersion": "realVersion2", "renderVersion": "renderVersion2" }
]
)";

    static const Config config{
        .map = {
            {.urlRegex = "urlRegex1", .versionRegex = "versionRegex1", .realVersion = "realVersion1"_ver, .renderVersion = "renderVersion1"_ver},
            {.urlRegex = "urlRegex2", .versionRegex = "versionRegex2", .realVersion = "realVersion2"_ver, .renderVersion = "renderVersion2"_ver},
        }};

    rapidjson::Document doc;
    rapidjson::MemoryStream stream{configJson, std::size(configJson)};
    doc.ParseStream(stream);
    ASSERT_FALSE(doc.HasParseError());
    const auto& configFromJson = Config::fromJson(doc);
    EXPECT_EQ(config, configFromJson);
}
