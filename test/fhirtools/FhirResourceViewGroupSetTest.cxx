/*
 * (C) Copyright IBM Deutschland GmbH 2024
 * (C) Copyright IBM Corp. 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "MockFhirResourceGroup.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

class FhirResourceViewGroupSetTest : public ::testing::Test
{
public:
    class Resolver : public ::testing::NiceMock<MockResourceGroupResolver>
    {
    public:
        std::shared_ptr<const fhirtools::FhirResourceGroup>
        findGroup(const std::string& url, const fhirtools::FhirVersion& version,
                  const std::filesystem::path& sourceFile) const override;
        std::shared_ptr<::testing::NiceMock<MockResourceGroup>> baseTypes =
            std::make_shared<::testing::NiceMock<MockResourceGroup>>();
        std::shared_ptr<::testing::NiceMock<MockResourceGroup>> typesA =
            std::make_shared<::testing::NiceMock<MockResourceGroup>>();
        std::shared_ptr<::testing::NiceMock<MockResourceGroup>> typesB =
            std::make_shared<::testing::NiceMock<MockResourceGroup>>();
    };

    void SetUp() override
    {
        using namespace fhirtools::version_literal;
        using ::testing::Eq;
        using ::testing::Return;

        // ON_CALL(resolver, allGroups).WillByDefault(Return({
        //     {"base", resolver.baseTypes},
        //     {"typesA", resolver.typesA},
        //     {"typesB", resolver.typesB},
        //
        // }));

        ON_CALL(*resolver.baseTypes, id).WillByDefault(Return("base"));
        ON_CALL(*resolver.typesA, id).WillByDefault(Return("typesA"));
        ON_CALL(*resolver.typesB, id).WillByDefault(Return("typesB"));

        ON_CALL(*resolver.baseTypes, findVersion(Eq("http://hl7.org/fhir/StructureDefinition/string")))
            .WillByDefault(Return(std::make_pair("0.1"_ver, resolver.baseTypes)));
        ON_CALL(*resolver.baseTypes, findVersion(Eq("http://hl7.org/fhir/StructureDefinition/uri")))
            .WillByDefault(Return(std::make_pair("0.1"_ver, resolver.baseTypes)));

        ON_CALL(*resolver.typesA, findVersion(Eq("http://erp-test.de/versiontest/ValueSet")))
            .WillByDefault(Return(std::make_pair("0.1"_ver, resolver.baseTypes)));
        ON_CALL(*resolver.typesB, findVersion(Eq("http://erp-test.de/versiontest/ValueSet")))
            .WillByDefault(Return(std::make_pair("0.2"_ver, resolver.baseTypes)));

        ON_CALL(*resolver.typesA, findVersion(Eq("http://erp-test.de/versiontest/CodeSystem")))
            .WillByDefault(Return(std::make_pair("A"_ver, resolver.baseTypes)));
        ON_CALL(*resolver.typesB, findVersion(Eq("http://erp-test.de/versiontest/CodeSystem")))
            .WillByDefault(Return(std::make_pair("B"_ver, resolver.baseTypes)));

        ON_CALL(*resolver.typesA, findVersion(Eq("http://erp-test.de/versiontest/StructureDefinition")))
            .WillByDefault(Return(std::make_pair("alpha"_ver, resolver.baseTypes)));
        ON_CALL(*resolver.typesB, findVersion(Eq("http://erp-test.de/versiontest/StructureDefinition")))
            .WillByDefault(Return(std::make_pair("beta"_ver, resolver.baseTypes)));

        backend.load({ResourceManager::getAbsoluteFilename("test/fhir-path/samples/version_samples.xml")}, resolver);
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClear(resolver.baseTypes.get());
        ::testing::Mock::VerifyAndClear(resolver.typesA.get());
        ::testing::Mock::VerifyAndClear(resolver.typesB.get());
    }

    void testStruct(fhirtools::FhirStructureRepository& view, const std::string& url,
                    const fhirtools::FhirVersion& version)
    {
        const fhirtools::FhirStructureDefinition* def = nullptr;
        ASSERT_NO_THROW(def = view.findStructure(fhirtools::DefinitionKey{url, version}));
        ASSERT_NE(def, nullptr);
        EXPECT_EQ(def->version(), version);
    }
    void testValueSet(fhirtools::FhirStructureRepository& view, const std::string& url,
                      const fhirtools::FhirVersion& version)
    {
        const fhirtools::FhirValueSet* vs = nullptr;
        ASSERT_NO_THROW(vs = view.findValueSet(fhirtools::DefinitionKey{url, version}));
        ASSERT_NE(vs, nullptr);
        EXPECT_EQ(vs->getVersion(), version);
    }
    void testCodeSystem(fhirtools::FhirStructureRepository& view, const std::string& url,
                        const fhirtools::FhirVersion& version)
    {
        const fhirtools::FhirCodeSystem* cs = nullptr;
        ASSERT_NO_THROW(cs = view.findCodeSystem(fhirtools::DefinitionKey{url, version}));
        ASSERT_NE(cs, nullptr);
        EXPECT_EQ(cs->getVersion(), version);
    }

    Resolver resolver;
    fhirtools::FhirStructureRepositoryBackend backend;
};

std::shared_ptr<const fhirtools::FhirResourceGroup>
FhirResourceViewGroupSetTest::Resolver::findGroup(const std::string& url, const fhirtools::FhirVersion& version,
                                                  const std::filesystem::path& sourceFile [[maybe_unused]]) const
{
    using namespace fhirtools::version_literal;
    if (url == "http://hl7.org/fhir/StructureDefinition/string" || url == "http://hl7.org/fhir/StructureDefinition/uri")
    {
        return baseTypes;
    }
    if (version == "A"_ver || version == "alpha"_ver || version == "0.1"_ver)
    {
        return typesA;
    }
    if (version == "B"_ver || version == "beta"_ver || version == "0.2"_ver)
    {
        return typesB;
    }
    ADD_FAILURE() << "unexepcted url/version requested: " << url << "|" << version;
    return nullptr;
}


TEST_F(FhirResourceViewGroupSetTest, findXYZ)
{
    using namespace fhirtools::version_literal;
    using GroupSet = std::set<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceGroup>>>;
    fhirtools::FhirResourceViewGroupSet A{"viewA", GroupSet{resolver.baseTypes, resolver.typesA}, &backend};
    EXPECT_NO_FATAL_FAILURE(testStruct(A, "http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver));
    EXPECT_NO_FATAL_FAILURE(testStruct(A, "http://hl7.org/fhir/StructureDefinition/uri", "0.1"_ver));
    EXPECT_NO_FATAL_FAILURE(testValueSet(A, "http://erp-test.de/versiontest/ValueSet", "0.1"_ver));
    EXPECT_NO_FATAL_FAILURE(testCodeSystem(A, "http://erp-test.de/versiontest/CodeSystem", "A"_ver));
    EXPECT_NO_FATAL_FAILURE(testStruct(A, "http://erp-test.de/versiontest/StructureDefinition", "alpha"_ver));

    fhirtools::FhirResourceViewGroupSet B{"viewB", GroupSet{resolver.baseTypes, resolver.typesB}, &backend};
    EXPECT_NO_FATAL_FAILURE(testStruct(B, "http://hl7.org/fhir/StructureDefinition/string", "0.1"_ver));
    EXPECT_NO_FATAL_FAILURE(testStruct(B, "http://hl7.org/fhir/StructureDefinition/uri", "0.1"_ver));
    EXPECT_NO_FATAL_FAILURE(testValueSet(B, "http://erp-test.de/versiontest/ValueSet", "0.2"_ver));
    EXPECT_NO_FATAL_FAILURE(testCodeSystem(B, "http://erp-test.de/versiontest/CodeSystem", "B"_ver));
    EXPECT_NO_FATAL_FAILURE(testStruct(B, "http://erp-test.de/versiontest/StructureDefinition", "beta"_ver));
}
