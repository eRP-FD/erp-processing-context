/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

TEST(FhirStructureRepositoryFixerTest, BackboneNotSliced)
{
    using namespace fhirtools;
    using namespace fhirtools::version_literal;
    const auto& slicedBaseFile =
        ResourceManager::getAbsoluteFilename("test/fhir-path/FhirStructureRepositoryFixer/BackboneNotSliced.xml");
    FhirStructureRepositoryBackend repo;
    repo.load({slicedBaseFile}, fhirtools::FhirResourceGroupConst{"test"});
    const FhirStructureDefinition* backboneNotSliced =
        repo.findDefinition("http://erp.test/BackboneNotSliced", "4.0.1"_ver);
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path(), "discriminator");
}

TEST(FhirStructureRepositoryFixerTest, NotSlicedFromElementType)
{
    using namespace fhirtools;
    using namespace fhirtools::version_literal;
    const auto& slicedBaseFile = ResourceManager::getAbsoluteFilename(
        "test/fhir-path/FhirStructureRepositoryFixer/NotSlicedFromElementType.xml");
    FhirStructureRepositoryBackend repo;
    repo.load({slicedBaseFile}, fhirtools::FhirResourceGroupConst{"test"});
    const FhirStructureDefinition* backboneNotSliced =
        repo.findDefinition("http://erp.test/NotSlicedFromElementType", "4.0.1"_ver);
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".subElement.sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_TRUE(slicing.has_value());
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path(), "discriminator");
}
