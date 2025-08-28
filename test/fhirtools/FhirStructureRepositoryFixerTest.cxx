/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

TEST(FhirStructureRepositoryFixerTest, BackboneNotSliced)
{
    using namespace fhirtools;
    using namespace fhirtools::version_literal;
    FhirStructureRepositoryBackend repo;
    repo.load({
        ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
        ResourceManager::getAbsoluteFilename("test/fhir-path/FhirStructureRepositoryFixer/BackboneNotSliced.xml"),
    }, fhirtools::FhirResourceGroupConst{"test"});
    const FhirStructureDefinition* backboneNotSliced =
        repo.findDefinition("http://erp.test/BackboneNotSliced", "4.0.1"_ver);
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_NE(slicing, nullptr);
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path(), "discriminator");
}

TEST(FhirStructureRepositoryFixerTest, NotSlicedFromElementType)
{
    using namespace fhirtools;
    using namespace fhirtools::version_literal;
    FhirStructureRepositoryBackend repo;
    repo.load({
        ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
        ResourceManager::getAbsoluteFilename("test/fhir-path/FhirStructureRepositoryFixer/NotSlicedFromElementType.xml"),

    }, fhirtools::FhirResourceGroupConst{"test"});
    const FhirStructureDefinition* backboneNotSliced =
        repo.findDefinition("http://erp.test/NotSlicedFromElementType", "4.0.1"_ver);
    ASSERT_NE(backboneNotSliced, nullptr);
    auto backboneNotSlicedSliced = backboneNotSliced->findElement(".subElement.sliced");
    ASSERT_NE(backboneNotSlicedSliced, nullptr);
    const auto& slicing = backboneNotSlicedSliced->slicing();
    ASSERT_NE(slicing, nullptr);
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path(), "discriminator");
}

TEST(FhirStructureRepositoryFixerTest, FixMissingDiscriminator)
{
    using namespace fhirtools;
    using namespace fhirtools::version_literal;
    FhirStructureRepositoryBackend repo;
    repo.load({
        ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
        ResourceManager::getAbsoluteFilename("test/fhir-path/slicing/profiles/fix_missing_discriminator.xml"),

    }, fhirtools::FhirResourceGroupConst{"test"});
    const FhirStructureDefinition* fixMissingDiscriminator =
    repo.findDefinition("http://fhir-tools.test/minifhirtypes/fix_missing_discriminator", "0.8.15"_ver);
    ASSERT_NE(fixMissingDiscriminator, nullptr);
    auto sliced = fixMissingDiscriminator->findElement(".extension");
    ASSERT_NE(sliced, nullptr);
    const auto& slicing = sliced->slicing();
    ASSERT_NE(slicing, nullptr);
    EXPECT_EQ(slicing->slicingRules(), fhirtools::FhirSlicing::SlicingRules::closed);
    ASSERT_EQ(slicing->discriminators().size(), 1);
    EXPECT_EQ(slicing->discriminators().front().type(), FhirSliceDiscriminator::DiscriminatorType::value);
    EXPECT_EQ(slicing->discriminators().front().path(), "url");
}
