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

void checkProfileVersion(
    ::std::string_view profile,
    ::std::string_view jsonValue,
    std::string_view versionStr)//NOLINT(readability-function-cognitive-complexity)
{
    const auto profileParts = String::split(jsonValue, '|');
    ASSERT_GE(profileParts.size(), 1);
    EXPECT_EQ(profileParts[0], ::std::string{profile});

    ASSERT_EQ(profileParts.size(), 2);
    EXPECT_EQ(profileParts[1], versionStr);
}

TEST(MetaDataTest, Construct)//NOLINT(readability-function-cognitive-complexity)
{
    const auto profileVersion = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();
    model::MetaData metaData(profileVersion);

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
    const auto version = model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>();

    if(model::ResourceVersion::deprecatedProfile(version))
    {
        const ::model::MetaData metaData(version);
        const auto& document = metaData.jsonDocument();

        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxTask",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/0/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/0"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/1"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/2"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/3"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/2/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxAuditEvent",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/3/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/StructureDefinition/ErxDevice",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/4/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
    }
    else
    {
        const auto patientenRechnungVersion = model::ResourceVersion::DeGematikErezeptPatientenrechnungR4::v1_0_0;
        const ::model::MetaData metaData(version);
        const auto& document = metaData.jsonDocument();

        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/0/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_InfoReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/0"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/1"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/2"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Representative",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/3"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReq",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/4"}).value(),
            ::model::ResourceVersion::v_str(patientenRechnungVersion)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReply",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/1/supportedProfile/5"}).value(),
            ::model::ResourceVersion::v_str(patientenRechnungVersion)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/2/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_AuditEvent",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/3/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Device",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/4/profile"}).value(),
            ::model::ResourceVersion::v_str(version)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_ChargeItem",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/5/profile"}).value(),
            ::model::ResourceVersion::v_str(patientenRechnungVersion)));
        EXPECT_NO_THROW(checkProfileVersion(
            "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Consent",
            document.getOptionalStringValue(::rapidjson::Pointer{"/rest/0/resource/6/profile"}).value(),
            ::model::ResourceVersion::v_str(patientenRechnungVersion)));
    }
}
