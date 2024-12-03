/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/MetaData.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


TEST(MetaDataTest, Construct)//NOLINT(readability-function-cognitive-complexity)
{
    model::MetaData metaData{};

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
    model::Timestamp referenceTimestamp;
    ResourceTemplates::Versions::GEM_ERP workflowVersion;
    ResourceTemplates::Versions::GEM_ERPCHRG chargeInfoVersion;
    ResourceTemplates::Versions::KBV_ERP kbvVersion;
    friend void PrintTo(const MetaDataProfileTestParam& data, std::ostream* os)
    {
        (*os) << data.name;
    }
};

class MetaDataProfileTest : public ::testing::TestWithParam<MetaDataProfileTestParam>
{
public:
    void checkProfile(const rapidjson::Value::ConstArray& resourceArray, bool isSupported,
                      std::string_view resourceType, std::string_view testProfile,
                      const fhirtools::FhirVersion& version)
    {
        static const rapidjson::Pointer typePtr{"/type"};
        static const rapidjson::Pointer profilePtr{"/profile"};
        static const rapidjson::Pointer supportedProfilePtr{"/supportedProfile"};
        const std::string profileAndVersion = std::string{testProfile}.append(1, '|').append(to_string(version));
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

    testutils::ShiftFhirResourceViewsGuard shift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    using namespace model::resource;
    static const rapidjson::Pointer restPtr{"/rest"};
    static const rapidjson::Pointer modePtr{"/mode"};
    static const rapidjson::Pointer resourcePtr{ElementName::path(elements::resource)};
    const ::model::MetaData metaData{GetParam().referenceTimestamp};
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
    using namespace structure_definition;
    for (const auto& workflowVer : ResourceTemplates::Versions::GEM_ERP_all)
    {
        bool isSupported = GetParam().workflowVersion == workflowVer;
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Task", task, workflowVer));
        // EXPECT_NO_FATAL_FAILURE(
        //     checkProfile(resourceArray, isSupported, "Communication", communicationInfoReq, workflowVer));
        EXPECT_NO_FATAL_FAILURE(
            checkProfile(resourceArray, isSupported, "Communication", communicationReply, workflowVer));
        EXPECT_NO_FATAL_FAILURE(
            checkProfile(resourceArray, isSupported, "Communication", communicationDispReq, workflowVer));
        EXPECT_NO_FATAL_FAILURE(
            checkProfile(resourceArray, isSupported, "Communication", communicationRepresentative, workflowVer));
        EXPECT_NO_FATAL_FAILURE(
            checkProfile(resourceArray, isSupported, "MedicationDispense", medicationDispense, workflowVer));
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "AuditEvent", auditEvent, workflowVer));
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Device", device, workflowVer));
    }
    for (const auto& chargeInfoVersion : ResourceTemplates::Versions::GEM_ERPCHRG_all)
    {
        bool isSupported = GetParam().chargeInfoVersion == chargeInfoVersion;
        EXPECT_NO_FATAL_FAILURE(
            checkProfile(resourceArray, isSupported, "Communication", communicationChargChangeReq, chargeInfoVersion));
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Communication", communicationChargChangeReply,
                                             chargeInfoVersion));
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "ChargeItem", chargeItem, chargeInfoVersion));
        EXPECT_NO_FATAL_FAILURE(checkProfile(resourceArray, isSupported, "Consent", consent, chargeInfoVersion));
    }
    if (HasFailure())
    {
        LOG(INFO) << metaData.serializeToJsonString();
    }
}

INSTANTIATE_TEST_SUITE_P(Combinations, MetaDataProfileTest,
                         ::testing::ValuesIn(std::list<MetaDataProfileTestParam>{
                             {
                                 .name = "v_2023_07_01",
                                 .referenceTimestamp = model::Timestamp::fromGermanDate("2023-07-01"),
                                 .workflowVersion = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .chargeInfoVersion = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_1_0,
                             },
                             {
                                 .name = "v_2024_10_01",
                                 .referenceTimestamp = model::Timestamp::fromGermanDate("2024-10-01"),
                                 .workflowVersion = ResourceTemplates::Versions::GEM_ERP_1_2,
                                 .chargeInfoVersion = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_1_0,
                             },
                             {
                                 .name = "v_2024_11_01",
                                 .referenceTimestamp = model::Timestamp::fromGermanDate("2024-11-01"),
                                 .workflowVersion = ResourceTemplates::Versions::GEM_ERP_1_3,
                                 .chargeInfoVersion = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_1_0,
                             },
                             {
                                 .name = "v_2025_01_15",
                                 .referenceTimestamp = model::Timestamp::fromGermanDate("2025-01-15"),
                                 .workflowVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
                                 .chargeInfoVersion = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_1_0,
                             },
                         }),
                         [](const auto& info) {
                             return std::string{info.param.name};
                         });
