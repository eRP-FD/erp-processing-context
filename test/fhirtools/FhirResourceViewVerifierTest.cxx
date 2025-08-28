/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/views/FhirResourceViewVerifier.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


TEST(FhirResourceViewVerifierTest, missingValueSetVersion)
{
    fhirtools::FhirStructureRepositoryBackend repo;
    fhirtools::FhirResourceGroupConst resolver{"test"};
    ASSERT_NO_THROW(repo.load(
        {
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/ERP-27211-missing-required-valueset-version.xml"),

        },
        resolver););
    auto view = fhirtools::FhirResourceViewGroupSet::create("test", resolver.findGroupById("test"), &repo);
    fhirtools::FhirResourceViewVerifier verifier{repo, view.get()};
    EXPECT_ANY_THROW(verifier.verify());
}
