#include "shared/fhir/Fhir.hxx"

#include <gtest/gtest.h>

TEST(FhirRepositoryTest, loadConfig)
{
    ASSERT_NO_THROW(Fhir::instance());
}
