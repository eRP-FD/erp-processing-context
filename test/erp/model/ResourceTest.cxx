/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/String.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <utility>

using namespace ::std::literals::string_view_literals;

namespace model
{

class FriendlyResourceBase : public UnspecifiedResource
{
    using UnspecifiedResource::UnspecifiedResource;

    FRIEND_TEST(ResourceBaseTest, Constructor);
    FRIEND_TEST(ResourceBaseTest, InvalidConstruction);
};

TEST(ResourceBaseTest, InvalidConstruction)//NOLINT(readability-function-cognitive-complexity)
{
    //NOLINTBEGIN(cppcoreguidelines-owning-memory)
    std::unique_ptr<FriendlyResourceBase> resource;
    EXPECT_ANY_THROW(resource.reset(new FriendlyResourceBase{fhirtools::DefinitionKey{""}}));
    EXPECT_ANY_THROW(
        resource.reset(new FriendlyResourceBase{fhirtools::DefinitionKey{"https://company.com/invalid/profile"}}));
    EXPECT_ANY_THROW(resource.reset(new FriendlyResourceBase{ProfileType::fhir}));
    //NOLINTEND(cppcoreguidelines-owning-memory)
}
} // namespace model


TEST(ResourceFactoryTest, invalid_profile)
{
    auto mdisp = ResourceTemplates::medicationDispenseXml({});
    mdisp = String::replaceAll(mdisp, "https://gematik.de/fhir/erp/StructureDefinition",
                               "https://invalid/fhir/erp/StructureDefinition");
    using Factory = model::ResourceFactory<model::MedicationDispense>;
    try {
        Factory::fromXml(mdisp, *StaticData::getXmlValidator())
            .getValidated(model::ProfileType::Gem_erxMedicationDispense);
        GTEST_FAIL() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        EXPECT_EQ(
            *ex.diagnostics(),
            R"(MedicationDispense: error: Unknown profile: https://invalid/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2; )"
            R"(MedicationDispense.meta.profile[0]: error: )"
            R"----(value must match fixed value: "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2")----"
            R"----( (but is "https://invalid/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2"))----"
            R"----( (from profile: https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense|1.2); )----");
    }
    catch (const std::exception& ex)
    {
        GTEST_FAIL() << "Unexpected Exception " << typeid(ex).name() << ": " << ex.what();
    }
}
