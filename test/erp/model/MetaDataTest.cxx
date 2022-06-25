/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/MetaData.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

void checkProfileVersion(::std::string_view profile, ::std::string_view jsonValue)//NOLINT(readability-function-cognitive-complexity)
{
    const auto profileParts = ::String::split(jsonValue, '|');
    ASSERT_GE(profileParts.size(), 1);
    EXPECT_EQ(profileParts[0], ::std::string{profile});

    if (::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>() ==
        ::model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1)
    {
        ASSERT_EQ(profileParts.size(), 1);
    }
    else
    {
        ASSERT_EQ(profileParts.size(), 2);
        EXPECT_EQ(profileParts[1],
                  ::model::ResourceVersion::v_str(
                      ::model::ResourceVersion::current<::model::ResourceVersion::DeGematikErezeptWorkflowR4>()));
    }
}

TEST(MetaDataTest, Construct)//NOLINT(readability-function-cognitive-complexity)
{
    model::MetaData metaData;

    EXPECT_EQ(metaData.version(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(metaData.date(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));
    EXPECT_EQ(metaData.releaseDate(), model::Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate));

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

TEST(MetaDataTest, ProfileVersions)//NOLINT(readability-function-cognitive-complexity)
{
    const auto now = ::model::Timestamp::now().toXsDateTime();
    EnvironmentVariableGuard enableProfile{"ERP_FHIR_PROFILE_VALID_FROM", now};
    EnvironmentVariableGuard renderProfile{"ERP_FHIR_PROFILE_RENDER_FROM", now};

    for (const auto& version : {"1.0.3-1", "1.1.1"})
    {
        EnvironmentVariableGuard forceGematikVersion{"ERP_FHIR_PROFILE_XML_SCHEMA_GEMATIK_VERSION", version};

        ::model::MetaData metaData;
        const auto& document = metaData.jsonDocument();
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxTask",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/0/profile"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/0"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/1"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/2"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/3"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/2/profile"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxAuditEvent",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/3/profile"}).value()));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxDevice",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/4/profile"}).value()));
    }
}
