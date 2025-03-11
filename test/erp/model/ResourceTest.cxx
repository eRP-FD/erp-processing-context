/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvBundle.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/GemErpPrMedication.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ErpException.hxx"
#include "shared/util/String.hxx"
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
    static constexpr auto makeBroken = [](const std::string& good) {
        return String::replaceAll(good, "https://fhir.kbv.de/StructureDefinition",
                                  "https://invalid/fhir/erp/StructureDefinition");
    };
    using namespace std::string_literals;
    const auto& fhirInstance = Fhir::instance();
    const auto& backend = fhirInstance.backend();
    auto supported =
        fhirInstance.structureRepository(model::Timestamp::now())
            .supportedVersions(&backend, {std::string{model::resource::structure_definition::prescriptionItem}});
    ASSERT_FALSE(supported.empty());
    std::string_view sep{};
    std::string profileList;
    for (const auto& profile: supported)
    {
        profileList += sep;
        profileList += to_string(profile);
        sep = ", ";
    }
    const auto goodProfile = to_string(std::ranges::max(supported, {}, &fhirtools::DefinitionKey::version));
    const auto badProfile = makeBroken(goodProfile);
    auto badMedicationDispense = makeBroken(ResourceTemplates::kbvBundleXml({}));
    using Factory = model::ResourceFactory<model::KbvBundle>;
    try {
        Factory::fromXml(badMedicationDispense, *StaticData::getXmlValidator())
            .getValidated(model::ProfileType::KBV_PR_ERP_Bundle);
        GTEST_FAIL() << "Expected ErpException";
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        EXPECT_EQ( *ex.diagnostics(), "invalid profile " + badProfile + " must be one of: " + profileList);
    }
    catch (const std::exception& ex)
    {
        GTEST_FAIL() << "Unexpected Exception " << typeid(ex).name() << ": " << ex.what();
    }
}
