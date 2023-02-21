/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

TEST(FhirStructureRepositoryTest, BackboneNotSliced)
{
    using namespace fhirtools;
    const auto& slicedBaseFile = ResourceManager::getAbsoluteFilename(
        "test/fhir-path/FhirStructureRepositoryFixer/BackboneNotSliced.xml");
    FhirStructureRepository repo;
    repo.load({slicedBaseFile});
    const FhirStructureDefinition* backboneNotSliced = repo.findDefinitionByUrl("http://erp.test/BackboneNotSliced");
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type() , FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path() , "discriminator");

}

TEST(FhirStructureRepositoryTest, NotSlicedFromElementType)
{
    using namespace fhirtools;
    const auto& slicedBaseFile = ResourceManager::getAbsoluteFilename(
        "test/fhir-path/FhirStructureRepositoryFixer/NotSlicedFromElementType.xml");
    FhirStructureRepository repo;
    repo.load({slicedBaseFile});
    const FhirStructureDefinition* backboneNotSliced = repo.findDefinitionByUrl("http://erp.test/NotSlicedFromElementType");
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".subElement.sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type() , FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path() , "discriminator");

}
