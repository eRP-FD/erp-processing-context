#include "fhirtools/repository/FhirResourceViewVerifier.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
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
    fhirtools::FhirResourceViewGroupSet view{"test", resolver.findGroupById("test"), &repo};
    fhirtools::FhirResourceViewVerifier verifier{repo, &view};
    EXPECT_ANY_THROW(verifier.verify());
}
