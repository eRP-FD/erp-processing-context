

#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"

#include <gtest/gtest.h>


TEST(ExporterConfigurationTest, loadProfiles)
{
    EXPECT_NO_THROW(Fhir::instance());
}

