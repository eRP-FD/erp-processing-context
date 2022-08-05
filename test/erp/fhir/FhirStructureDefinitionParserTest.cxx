/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/fhir/Fhir.hxx"
#include "fhirtools/repository/internal/FhirStructureDefinitionParser.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "test/util/ResourceManager.hxx"
#include "test_config.h"

using namespace fhirtools;

TEST(FhirStructureDefinitionParserTest, TestPrimitives) //NOLINT
{
    using namespace std::string_view_literals;
    using Kind = FhirStructureDefinition::Kind;
    using Derivation = FhirStructureDefinition::Derivation;
    using Representation = FhirElement::Representation;

    const auto& fhirResourcesXml = ResourceManager::getAbsoluteFilename("test/fhir/TestPrimitives.xml");
    auto [types, codeSystems, valueSets] = FhirStructureDefinitionParser::parse(fhirResourcesXml);
    ASSERT_EQ(types.size(), 2);
    ASSERT_TRUE(codeSystems.empty());
    ASSERT_TRUE(valueSets.empty());
    const FhirStructureDefinition* stringDef = nullptr;
    const FhirStructureDefinition* integerDef = nullptr;
    for (const auto& type: types)
    {
        if (type.typeId() == "string")
        {
            stringDef = &type;
        }
        else if (type.typeId() == "integer")
        {
            integerDef = &type;
        }
    }
    ASSERT_TRUE(stringDef != nullptr);
    EXPECT_EQ(stringDef->url(), "http://hl7.org/fhir/StructureDefinition/string"sv);
    EXPECT_EQ(stringDef->kind(), Kind::primitiveType);
    EXPECT_EQ(stringDef->baseDefinition(), "http://hl7.org/fhir/StructureDefinition/Element"sv);
    EXPECT_EQ(stringDef->derivation(), Derivation::specialization);

    const auto stringBaseElement = stringDef->findElement("string");
    ASSERT_TRUE(stringBaseElement);
    EXPECT_EQ(stringBaseElement->typeId(), "string"sv);
    EXPECT_EQ(stringBaseElement->name(), "string"sv);
    EXPECT_TRUE(stringBaseElement->isRoot());
    EXPECT_TRUE(stringBaseElement->isArray());

    const auto stringValueElement = stringDef->findElement("string.value");
    ASSERT_TRUE(stringValueElement);
    EXPECT_EQ(stringValueElement->typeId(), "http://hl7.org/fhirpath/System.String"sv);
    EXPECT_EQ(stringValueElement->name(), "string.value"sv);
    EXPECT_EQ(stringValueElement->representation(), Representation::xmlAttr);
    EXPECT_FALSE(stringValueElement->isRoot());
    EXPECT_FALSE(stringValueElement->isArray());


    ASSERT_TRUE(integerDef != nullptr);
    EXPECT_EQ(integerDef->url(), "http://hl7.org/fhir/StructureDefinition/integer"sv);
    EXPECT_EQ(integerDef->kind(), Kind::primitiveType);
    EXPECT_EQ(integerDef->baseDefinition(), "http://hl7.org/fhir/StructureDefinition/Element"sv);
    EXPECT_EQ(integerDef->derivation(), Derivation::specialization);

    const auto integerBaseElement = integerDef->findElement("integer");
    ASSERT_TRUE(integerBaseElement);
    EXPECT_EQ(integerBaseElement->typeId(), "integer"sv);
    EXPECT_EQ(integerBaseElement->name(), "integer"sv);
    EXPECT_TRUE(integerBaseElement->isRoot());
    EXPECT_TRUE(integerBaseElement->isArray());

    const auto integerValueElement = integerDef->findElement("integer.value");
    ASSERT_TRUE(integerValueElement);
    EXPECT_EQ(integerValueElement->typeId(), "http://hl7.org/fhirpath/System.Integer"sv);
    EXPECT_EQ(integerValueElement->name(), "integer.value"sv);
    EXPECT_EQ(integerValueElement->representation(), Representation::xmlAttr);
    EXPECT_FALSE(integerValueElement->isRoot());
    EXPECT_FALSE(integerValueElement->isArray());
}


TEST(FhirStructureDefinitionParserTest, InvalidXml) //NOLINT
{
    const auto& fhirResourcesXml = ResourceManager::getAbsoluteFilename("test/fhir/InvalidStructureDefinition.xml");
    ASSERT_ANY_THROW(FhirStructureDefinitionParser::parse(fhirResourcesXml));
}

TEST(FhirStructureDefinitionParserTest, SlicedByFixed) //NOLINT
{
    const auto& fhirResourcesXml =
        ResourceManager::getAbsoluteFilename("test/fhir-path/slicing/profiles/slice_by_fixed.xml");
    std::list<FhirStructureDefinition> defs;
    std::list<FhirCodeSystem> syss;
    std::list<FhirValueSet> vSets;
    ASSERT_NO_THROW(std::tie(defs,syss,vSets) = FhirStructureDefinitionParser::parse(fhirResourcesXml));
    ASSERT_EQ(defs.size(), 1);
    const auto& slicedByFixed = defs.front();
    std::shared_ptr<const FhirElement> sliced;
    ASSERT_NO_THROW(sliced = slicedByFixed.findElement(".sliced"));
    ASSERT_NE(sliced, nullptr);
    ASSERT_TRUE(sliced->hasSlices());
    EXPECT_EQ(sliced->typeId(), "Sliced");
    const auto& slicing = sliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    const auto& discriminators = slicing->discriminators();
    ASSERT_EQ(discriminators.size(), 1);
    ASSERT_EQ(discriminators.front().path(), "smallstructure.string");
    ASSERT_EQ(discriminators.front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    const auto& slices = slicing->slices();
    static auto checkSlice = [&](const FhirSlice& slice, const std::string& fixedString)
    {
        const auto& profile = slice.profile();
        EXPECT_EQ(profile.typeId(), "Sliceable.sliced");
        const auto& elements = profile.elements();
        ASSERT_EQ(elements.size(), 6);
        EXPECT_EQ(elements[0]->name(), "Sliceable.sliced");
        EXPECT_EQ(elements[1]->name(), "Sliceable.sliced.smallstructure");
        EXPECT_EQ(elements[2]->name(), "Sliceable.sliced.smallstructure.id");
        ASSERT_EQ(elements[3]->name(), "Sliceable.sliced.smallstructure.string");
        const auto& smallstructureString = elements[3];
        EXPECT_EQ(elements[4]->name(), "Sliceable.sliced.smallstructure.integer");
        EXPECT_EQ(elements[5]->name(), "Sliceable.sliced.smallstructure.coding");

        EXPECT_EQ(smallstructureString->pattern(), nullptr);
        const auto& fixed = smallstructureString->fixed();
        ASSERT_TRUE(fixed->children().empty());
        const auto& attr = fixed->attributes();
        ASSERT_EQ(attr.size(), 1);
        EXPECT_EQ(attr.begin()->first, "value");
        EXPECT_EQ(attr.begin()->second, fixedString);


    };
    ASSERT_EQ(slices.size(), 2);
    EXPECT_EQ(slices[0].name(), "slice1");
    EXPECT_EQ(slices[1].name(), "slice2");
    ASSERT_NO_FATAL_FAILURE(checkSlice(slices[0], "slice1"));
    ASSERT_NO_FATAL_FAILURE(checkSlice(slices[1], "slice2"));

}

TEST(FhirStructureDefinitionParserTest, SlicedByPattern) //NOLINT
{
    const auto& fhirResourcesXml =
        ResourceManager::getAbsoluteFilename("test/fhir-path/slicing/profiles/slice_by_pattern.xml");
    std::list<FhirStructureDefinition> defs;
    std::list<FhirCodeSystem> syss;
    std::list<FhirValueSet> vSets;
    ASSERT_NO_THROW(std::tie(defs,syss,vSets) = FhirStructureDefinitionParser::parse(fhirResourcesXml));
    ASSERT_EQ(defs.size(), 1);
    const auto& slicedByFixed = defs.front();
    std::shared_ptr<const FhirElement> sliced;
    ASSERT_NO_THROW(sliced = slicedByFixed.findElement(".sliced"));
    ASSERT_NE(sliced, nullptr);
    ASSERT_TRUE(sliced->hasSlices());
    EXPECT_EQ(sliced->typeId(), "Sliced");
    const auto& slicing = sliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    const auto& discriminators = slicing->discriminators();
    ASSERT_EQ(discriminators.size(), 1);
    ASSERT_EQ(discriminators.front().path(), "smallstructure.coding");
    ASSERT_EQ(discriminators.front().type(), FhirSliceDiscriminator::DiscriminatorType::pattern);
    const auto& slices = slicing->slices();
    static auto checkSlice = [&](const FhirSlice& slice, const std::string& codingSystem)
    {
        const auto& profile = slice.profile();
        EXPECT_EQ(profile.typeId(), "Sliceable.sliced");
        const auto& elements = profile.elements();
        ASSERT_EQ(elements.size(), 6);
        EXPECT_EQ(elements[0]->name(), "Sliceable.sliced");
        EXPECT_EQ(elements[1]->name(), "Sliceable.sliced.smallstructure");
        EXPECT_EQ(elements[2]->name(), "Sliceable.sliced.smallstructure.id");
        ASSERT_EQ(elements[3]->name(), "Sliceable.sliced.smallstructure.string");
        EXPECT_EQ(elements[4]->name(), "Sliceable.sliced.smallstructure.integer");
        EXPECT_EQ(elements[5]->name(), "Sliceable.sliced.smallstructure.coding");
        const auto& smallstructureCoding = elements[5];

        EXPECT_EQ(smallstructureCoding->fixed(), nullptr);
        const auto& pattern = smallstructureCoding->pattern();
        ASSERT_TRUE(pattern->attributes().empty());
        const auto& children = pattern->children();
        ASSERT_EQ(children.size(), 1);
        EXPECT_EQ(children.begin()->first, "system");
        const auto& system = children.begin()->second;
        ASSERT_TRUE(system->children().empty());
        const auto& attr = system->attributes();
        ASSERT_EQ(attr.size(), 1);
        ASSERT_EQ(attr.begin()->first, "value");
        EXPECT_EQ(attr.begin()->second, codingSystem);
    };
    ASSERT_EQ(slices.size(), 2);
    EXPECT_EQ(slices[0].name(), "slice1");
    EXPECT_EQ(slices[1].name(), "slice2");
    ASSERT_NO_FATAL_FAILURE(checkSlice(slices[0], "http://erp.test/slice1/code"));
    ASSERT_NO_FATAL_FAILURE(checkSlice(slices[1], "http://erp.test/slice2/code"));

}

static bool containsCode(const std::vector<FhirCodeSystem::Code>& codes, const std::string& code)
{
    return std::find_if(codes.begin(), codes.end(), [code](const auto& c){return c.code == code;}) != codes.end();
}

TEST(FhirStructureDefinitionParserTest, CodeSystems)
{
    const auto& fhirCodeSystemsXml = ResourceManager::getAbsoluteFilename("test/fhir/TestCodeSystem.xml");
    std::list<FhirStructureDefinition> defs;
    std::list<FhirCodeSystem> codeSystems;
    std::list<FhirValueSet> vSets;
    ASSERT_NO_THROW(std::tie(defs,codeSystems,vSets) = FhirStructureDefinitionParser::parse(fhirCodeSystemsXml));
    ASSERT_TRUE(defs.empty());
    ASSERT_TRUE(vSets.empty());
    ASSERT_EQ(codeSystems.size(), 3);
    EXPECT_EQ(codeSystems.front().getUrl(), "https://test/CodeSystem1");
    EXPECT_EQ(codeSystems.front().getName(), "TEST_CODESYSTEM1");
    EXPECT_TRUE(codeSystems.front().isCaseSensitive());
    EXPECT_TRUE(containsCode(codeSystems.front().getCodes(), "00"));
    EXPECT_TRUE(containsCode(codeSystems.front().getCodes(), "01"));
    EXPECT_TRUE(containsCode(codeSystems.front().getCodes(), "02"));
    EXPECT_TRUE(containsCode(codeSystems.front().getCodes(), "03"));

    EXPECT_EQ(codeSystems.back().getUrl(), "https://test/Codesystem3");
    EXPECT_EQ(codeSystems.back().getName(), "TEST_CODESYSTEM3");
    EXPECT_FALSE(codeSystems.back().isCaseSensitive());
    EXPECT_TRUE(containsCode(codeSystems.back().getCodes(), "_ActCredentialedCareCode"));
    EXPECT_TRUE(containsCode(codeSystems.back().getCodes(), "SUB_1"));
    EXPECT_TRUE(containsCode(codeSystems.back().getCodes(), "SUB_2"));
}

TEST(FhirStructureDefinitionParserTest, ValueSets)
{
    const auto& fhirCodeSystemsXml = ResourceManager::getAbsoluteFilename("test/fhir/TestCodeSystem.xml");
    const auto& fhirValueSetsXml = ResourceManager::getAbsoluteFilename("test/fhir/TestValueSet.xml");
    std::list<FhirCodeSystem> codeSystems;
    std::list<FhirValueSet> vSets;
    ASSERT_NO_THROW(std::tie(std::ignore,codeSystems, std::ignore) = FhirStructureDefinitionParser::parse(fhirCodeSystemsXml));
    ASSERT_NO_THROW(std::tie(std::ignore,std::ignore, vSets) = FhirStructureDefinitionParser::parse(fhirValueSetsXml));
    ASSERT_EQ(vSets.size(), 4);
    ASSERT_EQ(codeSystems.size(), 3);
    std::vector<FhirValueSet> valueSets(vSets.begin(), vSets.end());
    auto& valueSet1 = valueSets[0];
    auto& valueSet2 = valueSets[1];
    auto& valueSet3 = valueSets[2];
    auto& valueSet4 = valueSets[3];

    EXPECT_EQ(valueSet1.getName(), "TEST_VALUESET1");
    EXPECT_EQ(valueSet1.getUrl(), "https://test/ValueSet1");
    const auto& valueSet1Includes = valueSet1.getIncludes();
    ASSERT_EQ(valueSet1Includes.size(), 1);
    EXPECT_EQ(valueSet1Includes[0].codeSystemUrl, "https://test/CodeSystem1");
    EXPECT_TRUE(valueSet1Includes[0].codes.empty());

    EXPECT_EQ(valueSet2.getName(), "TEST_VALUESET2");
    EXPECT_EQ(valueSet2.getUrl(), "https://test/ValueSet2");
    const auto& valueSet2Includes = valueSet2.getIncludes();
    ASSERT_EQ(valueSet2Includes.size(), 1);
    EXPECT_EQ(valueSet2Includes[0].codeSystemUrl, "https://test/CodeSystem1");
    ASSERT_FALSE(valueSet2Includes[0].codes.empty());
    EXPECT_EQ(valueSet2Includes[0].codes.size(), 2);
    EXPECT_TRUE(valueSet2Includes[0].codes.contains("00"));
    EXPECT_TRUE(valueSet2Includes[0].codes.contains("03"));

    EXPECT_EQ(valueSet3.getName(), "TEST_VALUESET3");
    EXPECT_EQ(valueSet3.getUrl(), "https://test/ValueSet3");
    const auto& valueSet3Includes = valueSet3.getIncludes();
    ASSERT_EQ(valueSet3Includes.size(), 2);
    EXPECT_EQ(valueSet3Includes[0].codeSystemUrl, "https://test/CodeSystem1");
    ASSERT_TRUE(valueSet3Includes[0].codes.empty());
    EXPECT_EQ(valueSet3Includes[1].codeSystemUrl, "https://test/CodeSystem2");
    ASSERT_FALSE(valueSet3Includes[1].codes.empty());
    EXPECT_TRUE(valueSet3Includes[1].codes.contains("UK"));

    EXPECT_EQ(valueSet4.getName(), "TEST_VALUESET4");
    ASSERT_EQ(valueSet4.getIncludes().size(), 1);
    ASSERT_EQ(valueSet4.getIncludes()[0].filters.size(), 1);
    EXPECT_EQ(valueSet4.getIncludes()[0].filters[0].mValue, "_ActCredentialedCareCode");
    EXPECT_EQ(valueSet4.getIncludes()[0].filters[0].mOp, FhirValueSet::FilterOp::isA);
}

TEST(FhirStructureDefinitionParserTest, contentReference)
{
    const auto& repo = Fhir::instance().structureRepository();
    const auto* bundleStructDef = repo.findTypeById("Bundle");
    ASSERT_NE(bundleStructDef, nullptr);
    const auto& entryLinkElementDef = bundleStructDef->findElement("Bundle.entry.link");
    ASSERT_NE(entryLinkElementDef, nullptr);
    ASSERT_TRUE(entryLinkElementDef->typeId().empty());
    std::optional<fhirtools::FhirStructureRepository::ContentReferenceResolution> resolution;
    ASSERT_NO_THROW(resolution.emplace(repo.resolveContentReference(*entryLinkElementDef)));
    ASSERT_NE(resolution->element, nullptr);
    EXPECT_EQ(resolution->element->name(), "Bundle.link");
}
