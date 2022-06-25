/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Resource.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/String.hxx"

#include <gtest/gtest.h>
#include <array>
#include <memory>
#include <utility>

using namespace ::std::literals::string_view_literals;

namespace model
{

class FriendlyResourceBase : public ResourceBase
{
    using ResourceBase::ResourceBase;

    FRIEND_TEST(ResourceBaseTest, Constructor);
};

TEST(ResourceBaseTest, Constructor)//NOLINT(readability-function-cognitive-complexity)
{
    const auto [gematikVersion, kbvVersion] = ResourceVersion::current();

    const ::std::array<::std::pair<::std::string_view, ::std::string_view>, 3> profileTypes = {
        ::std::make_pair("https://gematik.de/fhir/StructureDefinition/ErxReceipt"sv,
                         ResourceVersion::v_str(gematikVersion)),
        ::std::make_pair("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle"sv,
                         ResourceVersion::v_str(kbvVersion)),
        ::std::make_pair("http://hl7.org/fhir/StructureDefinition/Binary"sv, "4.0.1"sv)};


    ::std::unique_ptr<FriendlyResourceBase> resource;
    for (const auto& type : profileTypes)
    {
        ASSERT_NO_THROW(resource.reset(new FriendlyResourceBase{type.first}));
        const auto profile = resource->getOptionalStringValue(::rapidjson::Pointer{"/meta/profile/0"});
        ASSERT_TRUE(profile);
        if (ResourceVersion::current<ResourceVersion::DeGematikErezeptWorkflowR4>() ==
            ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1)
        {
            const auto profileParts = ::String::split(profile.value(), '|');
            ASSERT_EQ(profileParts.size(), 1);
            EXPECT_EQ(profileParts[0], type.first);
        }
        else
        {
            const auto profileParts = ::String::split(profile.value(), '|');
            ASSERT_EQ(profileParts.size(), 2);
            EXPECT_EQ(profileParts[0], type.first);
            EXPECT_EQ(profileParts[1], type.second);
        }
    }

    {
        EXPECT_THROW(resource.reset(new FriendlyResourceBase{""}), ::std::logic_error);
        EXPECT_THROW(resource.reset(new FriendlyResourceBase{"https://company.com/invalid/profile"}), ::std::logic_error);
    }
}
}
