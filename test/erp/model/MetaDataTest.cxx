/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/MetaData.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


TEST(MetaDataTest, Construct)//NOLINT(readability-function-cognitive-complexity)
{
    model::MetaData metaData(model::ResourceVersion::currentBundle());

    EXPECT_EQ(metaData.version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(metaData.date(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate().data()));
    EXPECT_EQ(metaData.releaseDate(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate().data()));

    ASSERT_NO_THROW(StaticData::getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(metaData.jsonDocument()), SchemaType::fhir));

    const auto* version = "1.1.012a";
    const model::Timestamp releaseDate = model::Timestamp::now();
    EXPECT_NO_THROW(metaData.setVersion(version));
    EXPECT_NO_THROW(metaData.setDate(releaseDate));
    EXPECT_NO_THROW(metaData.setReleaseDate(releaseDate));
    EXPECT_EQ(metaData.version(), version);
    EXPECT_EQ(metaData.releaseDate(), releaseDate);
    EXPECT_EQ(metaData.date(), releaseDate);
}

struct MetaDataProfileTestParam
{
    std::string_view name;
    model::ResourceVersion::FhirProfileBundleVersion profileBundle;
};

class MetaDataProfileTest : public ::testing::TestWithParam<MetaDataProfileTestParam>
{
public:

    template <typename SchemaVersionType>
    void checkProfile(const rapidjson::Value::ConstArray& resourceArray, bool isSupported,
                      std::string_view resourceType, std::string_view testProfile, SchemaVersionType version)
    {
        static const rapidjson::Pointer typePtr{"/type"};
        static const rapidjson::Pointer profilePtr{"/profile"};
        static const rapidjson::Pointer supportedProfilePtr{"/supportedProfile"};
        const std::string profileAndVersion = std::string{testProfile}.append(1,'|')
                                              .append(model::ResourceVersion::v_str(version));
        bool foundType = false;
        bool foundProfile = false;
        for (const auto& res: resourceArray)
        {
            ASSERT_TRUE(model::NumberAsStringParserDocument::valueIsObject(res));
            const auto* typeValue = typePtr.Get(res);
            ASSERT_NE(typeValue, nullptr);
            std::string type;
            EXPECT_NO_THROW(type = model::NumberAsStringParserDocument::valueAsString(*typeValue));
            if (type == resourceType)
            {
                EXPECT_FALSE(foundType) << "resource type contained more than once.";
                foundType = true;
                const auto* profileValue = profilePtr.Get(res);
                ASSERT_NE(profileValue, nullptr);
                ASSERT_TRUE(model::NumberAsStringParserDocument::valueIsString(*profileValue));
                std::string profile;
                EXPECT_NO_THROW(profile = model::NumberAsStringParserDocument::valueAsString(*profileValue));
                if (profile == profileAndVersion)
                {
                    EXPECT_FALSE(foundProfile) << "profile found more than once.";
                    foundProfile = true;
                }
                const auto* supportedProfileValue = supportedProfilePtr.Get(res);
                if (supportedProfileValue)
                {
                    ASSERT_TRUE(supportedProfileValue->IsArray());
                    auto supportedProfileArray = supportedProfileValue->GetArray();
                    for (const auto& supportedProfileValue : supportedProfileArray)
                    {
                        ASSERT_TRUE(model::NumberAsStringParserDocument::valueIsString(supportedProfileValue));
                        std::string supportedProfile;
                        EXPECT_NO_THROW(supportedProfile = model::NumberAsStringParserDocument::valueAsString(supportedProfileValue));
                        if (supportedProfile == profileAndVersion)
                        {
                            EXPECT_FALSE(foundProfile) << "profile found more than once.";
                            foundProfile = true;
                        }
                    }
                }
            }
        }
        EXPECT_EQ(foundProfile, isSupported) << "testing for profile " << profileAndVersion << " with resourceType " << resourceType;
    }


};

TEST_P(MetaDataProfileTest, ProfileVersions)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace model::resource;
    namespace ResVer = model::ResourceVersion;
    static const rapidjson::Pointer restPtr{"/rest"};
    static const rapidjson::Pointer modePtr{"/mode"};
    static const rapidjson::Pointer resourcePtr{ElementName::path(elements::resource)};
    const ::model::MetaData metaData(GetParam().profileBundle);
    const auto& document = metaData.jsonDocument();
    const auto* restValue = restPtr.Get(document);
    ASSERT_NE(restValue, nullptr);
    ASSERT_TRUE(restValue->IsArray());
    const auto restArray = restValue->GetArray();
    ASSERT_EQ(restArray.Size(), 1);
    const auto* modeValue = modePtr.Get(restArray[0]);
    ASSERT_NE(modeValue, nullptr);
    std::string mode;
    EXPECT_NO_THROW(mode = model::NumberAsStringParserDocument::valueAsString(*modeValue));
    EXPECT_EQ(mode, "server");
    const auto* resourceValue = resourcePtr.Get(restArray[0]);
    ASSERT_NE(resourceValue, nullptr);
    ASSERT_TRUE(resourceValue->IsArray());
    auto resourceArray = resourceValue->GetArray();
    for (auto bundle : model::ResourceVersion::allBundles())
    {
        bool isSupported = GetParam().profileBundle == bundle;
        auto allVersions = ResVer::profileVersionFromBundle(bundle);
        auto workflowVer = get<ResVer::DeGematikErezeptWorkflowR4>(allVersions);
        if(model::ResourceVersion::deprecatedBundle(bundle))
        {
            using namespace structure_definition::deprecated;
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationInfoReq, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationReply, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationDispReq, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationRepresentative, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Task", task, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "MedicationDispense", medicationDispense, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "AuditEvent", auditEvent, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Device", device, workflowVer));
        }
        else
        {
            using namespace structure_definition;
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Task", task, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationInfoReq, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationReply, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationDispReq, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationRepresentative, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "MedicationDispense", medicationDispense, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "AuditEvent", auditEvent, workflowVer));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Device", device, workflowVer));
            auto pkvVersion = get<ResVer::DeGematikErezeptPatientenrechnungR4>(allVersions);
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationChargChangeReq, pkvVersion));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationChargChangeReply, pkvVersion));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "ChargeItem", chargeItem, pkvVersion));
            EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Consent", consent, pkvVersion));
        }
    }
}
// clang-format off
INSTANTIATE_TEST_SUITE_P(Combinations, MetaDataProfileTest, ::testing::Values(
    MetaDataProfileTestParam{"v_2022_01_01", model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01},
    MetaDataProfileTestParam{"v_2023_07_01", model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01}),
[](const auto& info) { return std::string{info.param.name}; }
);
// clang-format on
