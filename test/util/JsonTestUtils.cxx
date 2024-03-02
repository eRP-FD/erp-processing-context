/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "JsonTestUtils.hxx"
#include "TestUtils.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/ResourceNames.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using namespace model;
using namespace model::resource;

namespace {
void canonicalJsonInternal(std::string& jsonOut, const std::string& jsonIn)
{
    using std::min;
    rapidjson::Document jsonDoc;
    rapidjson::ParseResult result = jsonDoc.Parse(jsonIn);
    if (result.IsError())
    {
        std::string nearStr = jsonIn.substr(result.Offset(), min(jsonIn.size() - result.Offset(), static_cast<size_t>(10)));
        FAIL() << "JSON Parse Error '" << rapidjson::GetParseError_En(result.Code()) << "' near: " << nearStr;
    }
    jsonOut = jsonToString(jsonDoc);
}

void jsonToStringInternal(std::string& jsonOut, const rapidjson::Document& jsonDoc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    ASSERT_TRUE(jsonDoc.Accept(writer)) << "failed to serialize JSON.";
    jsonOut = buffer.GetString();
}
}

std::string jsonToString(const rapidjson::Document& json)
{
    std::string result;
    jsonToStringInternal(result, json);
    return result;
}

std::string canonicalJson(const std::string& json)
{
    std::string result;
    canonicalJsonInternal(result, json);
    return result;
}

const std::map<ActorRole, std::string_view> ActorRoleToResourceIdDeprecated = {
    { ActorRole::Insurant,       naming_system::deprecated::gkvKvid10   },
    { ActorRole::Representative, naming_system::deprecated::gkvKvid10   },
    { ActorRole::Doctor,         naming_system::deprecated::telematicID },
    { ActorRole::Pharmacists,    naming_system::deprecated::telematicID }
};

const std::map<ActorRole, std::string_view> ActorRoleToResourceId = {
    { ActorRole::Insurant,       naming_system::gkvKvid10   },
    { ActorRole::Representative, naming_system::gkvKvid10   },
    { ActorRole::Doctor,         naming_system::telematicID },
    { ActorRole::Pharmacists,    naming_system::telematicID }
};

const std::string_view& actorRoleToResourceId(ActorRole actorRole,
                                              ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
{
    if (ResourceVersion::deprecatedProfile(profileVersion))
        return ActorRoleToResourceIdDeprecated.find(actorRole)->second;
    return ActorRoleToResourceId.find(actorRole)->second;
}

CommunicationJsonStringBuilder::CommunicationJsonStringBuilder(Communication::MessageType messageType, model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion) :
    mMessageType(messageType),
    mPrescriptionId(),
    mAccessCode(),
    mSender(),
    mRecipient(),
    mTimeSent(),
    mTimeReceived(),
    mPayload(),
    mProfileVersion(profileVersion)
{
}

CommunicationJsonStringBuilder::~CommunicationJsonStringBuilder() = default;


CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setPrescriptionId(const std::string& prescriptionId)
{
    mPrescriptionId = prescriptionId;
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setAccessCode(const std::string& accessCode)
{
    mAccessCode = accessCode;
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setRecipient(ActorRole actorRole, const std::string& recipient)
{
    mRecipient = std::make_tuple(actorRole, recipient);
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setSender(ActorRole actorRole, const std::string& sender)
{
    mSender = std::make_tuple(actorRole, sender);
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setTimeSent(const std::string& timeSent)
{
    mTimeSent = timeSent;
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setTimeReceived(const std::string& timeReceived)
{
    mTimeReceived = timeReceived;
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setPayload(const std::string& contentString)
{
    mPayload = contentString;
    return *this;
}

CommunicationJsonStringBuilder& CommunicationJsonStringBuilder::setAbout(const std::string& about)
{
    mAbout = about;
    return *this;
}

CommunicationJsonStringBuilder&
CommunicationJsonStringBuilder::setMedicationOptions(ResourceTemplates::MedicationOptions medicationOpts)
{
    mMedicationOptions = std::make_unique<ResourceTemplates::MedicationOptions>(std::move(medicationOpts));
    return *this;
}


std::string CommunicationJsonStringBuilder::createJsonString() const
{
    const auto fhirBundleVersion = ResourceVersion::fhirProfileBundleFromSchemaVersion(mProfileVersion);
    const auto profileVersions = ResourceVersion::profileVersionFromBundle(fhirBundleVersion);
    const SchemaType schemaType = model::Communication::messageTypeToSchemaType(mMessageType);

    static constexpr const char* fmtSpecProfile = R"(
            "meta": {"profile": ["%1%"]})";
    static constexpr const char* fmtSpecBasedOnTaskId = R"(
            "basedOn": [{"reference": "Task/%1%"}])";
    static constexpr const char* fmtSpecBasedOnTaskIdAccessCode = R"(
            "basedOn": [{"reference": "Task/%1%/$accept?ac=%2%"}])";
    static constexpr const char* fmtSpecBasedOnChargeItemId = R"(
            "basedOn": [{"reference": "ChargeItem/%1%"}])";
    static constexpr const char* fmtSpecAbout = R"(
            "about": [{"reference": "%1%"}])";
    static constexpr const char* fmtSpecSender = R"(
            "sender": {"identifier": {"system": "%1%","value" : "%2%"}})";
    static constexpr const char* fmtSpecRecipient = R"(
            "recipient": [{"identifier": {"system": "%1%","value" : "%2%"}}])";
    static constexpr const char* fmtSpecSent = R"(
            "sent": "%1%")";
    static constexpr const char* fmtSpecReceived = R"(
            "received": "%1%")";
    static constexpr const char* fmtSpecPayloadContentStringDeprecated = R"--(
            "payload": [
              {
                "extension": [
                  {
                    "url": "https://gematik.de/fhir/StructureDefinition/InsuranceProvider",
                    "valueIdentifier": {
                      "system": "http://fhir.de/NamingSystem/arge-ik/iknr",
                      "value": "104212059"
                    }
                  },
                  {
                    "url": "https://gematik.de/fhir/StructureDefinition/SubstitutionAllowedType",
                    "valueBoolean": true
                  },
                  {
                    "url": "https://gematik.de/fhir/StructureDefinition/PrescriptionType",
                    "valueCoding": {
                      "system": "https://gematik.de/fhir/CodeSystem/Flowtype",
                      "code": "160",
                      "display": "Muster 16 (Apothekenpflichtige Arzneimittel)"
                    }
                  }
                ],
                "contentString": "%1%"
              }
            ])--";
    static constexpr const char* fmtSpecPayloadContentString = R"--(
            "payload": [
              {
                "contentString": "%1%"
              }
            ])--";
    static constexpr const char* fmtSpecPayloadInfoReqContentString = R"--(
            "payload": [
              {
                "extension":  [
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_InsuranceProvider",
                        "valueIdentifier": {
                            "system": "http://fhir.de/sid/arge-ik/iknr",
                            "value": "109500969"
                        }
                    },
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_SubstitutionAllowedType",
                        "valueBoolean": false
                    },
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PrescriptionType",
                        "valueCoding": {
                            "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType",
                            "code": "160"
                        }
                    },
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PackageQuantity",
                        "valueQuantity": {
                            "system": "http://unitsofmeasure.org",
                            "value": 1
                        }
                    },
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_SupplyOptionsType",
                        "extension":  [
                            {
                                "url": "onPremise",
                                "valueBoolean": true
                            },
                            {
                                "url": "shipment",
                                "valueBoolean": false
                            },
                            {
                                "url": "delivery",
                                "valueBoolean": true
                            }
                        ]
                    }
                ],
                "contentString": "%1%"
              }
            ])--";
    std::string body = R"({"resourceType": "Communication",)";

    const bool isChargeItemRelated =
        mMessageType == model::Communication::MessageType::ChargChangeReq || mMessageType == model::Communication::MessageType::ChargChangeReply;

    body += boost::str(boost::format(fmtSpecProfile) % ::testutils::profile(schemaType, fhirBundleVersion));

    if (mPrescriptionId.has_value() && mAccessCode.has_value())
        body += "," + boost::str(boost::format(fmtSpecBasedOnTaskIdAccessCode) % mPrescriptionId.value() % mAccessCode.value());
    else if (mPrescriptionId.has_value())
        body += "," + boost::str(boost::format(isChargeItemRelated ? fmtSpecBasedOnChargeItemId : fmtSpecBasedOnTaskId) % mPrescriptionId.value());
    body += R"(, "status":"unknown")";
    if (mAbout.has_value())
        body += "," + boost::str(boost::format(fmtSpecAbout) % mAbout.value());
    if (mSender.has_value())
        body += "," + boost::str(boost::format(fmtSpecSender)
            % actorRoleToResourceId(std::get<0>(mSender.value()), mProfileVersion)
            % std::get<1>(mSender.value()));
    if (mRecipient.has_value())
        body += "," + boost::str(boost::format(fmtSpecRecipient)
            % actorRoleToResourceId(std::get<0>(mRecipient.value()), mProfileVersion)
            % std::get<1>(mRecipient.value()));
    if (mTimeSent.has_value())
        body += "," + boost::str(boost::format(fmtSpecSent) % mTimeSent.value());
    if (mTimeReceived.has_value())
        body += "," + boost::str(boost::format(fmtSpecReceived) % mTimeReceived.value());
    if (mPayload.has_value())
    {
        auto payload = String::replaceAll(mPayload.value(), "\"", "\\\"");
        if (ResourceVersion::deprecatedProfile(mProfileVersion))
            body += "," + boost::str(boost::format(fmtSpecPayloadContentStringDeprecated) % payload);
        else if (mMessageType == Communication::MessageType::InfoReq)
            body += "," + boost::str(boost::format(fmtSpecPayloadInfoReqContentString) % payload);
        else
            body += "," + boost::str(boost::format(fmtSpecPayloadContentString) % payload);
    }
    if (mAbout)
    {
        ResourceTemplates::MedicationOptions medicationOptions;
        if (mMedicationOptions)
        {
            medicationOptions = *mMedicationOptions;
        }
        else
        {
            std::string id{*mAbout};
            if (! id.empty() && id[0] == '#')
            {
                id = id.substr(1);
            }
            medicationOptions.version = get<ResourceVersion::KbvItaErp>(profileVersions);
            medicationOptions.id = id;
        }
        body += R"(, "contained": [)" +
                Fhir::instance()
                    .converter()
                    .xmlStringToJson(ResourceTemplates::medicationXml(medicationOptions))
                    .serializeToJsonString() +
                ']';
    }
    body += "}";
    return body;
}

std::string CommunicationJsonStringBuilder::createXmlString() const
{
    const auto fhirBundleVersion = ResourceVersion::fhirProfileBundleFromSchemaVersion(mProfileVersion);
    const auto profileVersions = ResourceVersion::profileVersionFromBundle(fhirBundleVersion);
    const SchemaType schemaType = model::Communication::messageTypeToSchemaType(mMessageType);

    static constexpr const char* fmtSpecProfile = R"(
            <meta> <profile value="%1%" /></meta>)";
    static constexpr const char* fmtSpecBasedOnTaskId = R"(
            <basedOn> <reference value="Task/%1%"/> </basedOn>)";
    static constexpr const char* fmtSpecBasedOnTaskIdAccessCode = R"(
            <basedOn> <reference value="Task/%1%/$accept?ac=%2%"/> </basedOn>)";
    static constexpr const char* fmtSpecBasedOnChargeItemId = R"(
            <basedOn> <reference value="ChargeItem/%1%"/> </basedOn>)";
    static constexpr const char* fmtSpecAbout = R"(
            <about> <reference value="%1%"/> </about>)";
    static constexpr const char* fmtSpecSender = R"(
            <sender> <identifier> <system value="%1%"/> <value value="%2%"/> </identifier> </sender>)";
    static constexpr const char* fmtSpecRecipient = R"(
            <recipient> <identifier> <system value="%1%"/> <value value="%2%"/> </identifier> </recipient>)";
    static constexpr const char* fmtSpecSent = R"(
            <sent value="%1%"/>)";
    static constexpr const char* fmtSpecReceived = R"(
            <received value="%1%"/>)";
    static constexpr const char* fmtSpecPayloadContentStringDeprecated = R"--(
            <payload>
              <extension url="https://gematik.de/fhir/StructureDefinition/InsuranceProvider">
                <valueIdentifier>
                  <system value="http://fhir.de/NamingSystem/arge-ik/iknr"/>
                  <value value="104212059"/>
                </valueIdentifier>
              </extension>
              <extension url="https://gematik.de/fhir/StructureDefinition/SubstitutionAllowedType">
                <valueBoolean value="true"/>
              </extension>
              <extension url="https://gematik.de/fhir/StructureDefinition/PrescriptionType">
                <valueCoding>
                  <system value="https://gematik.de/fhir/CodeSystem/Flowtype"/>
                  <code value="160"/>
                  <display value="Muster 16 (Apothekenpflichtige Arzneimittel)"/>
                </valueCoding>
              </extension>
              <contentString value="%1%"/>
            </payload>)--";
    static constexpr const char* fmtSpecPayloadContentString = R"--(
            <payload>
              <contentString value="%1%" />
            </payload>)--";
    static constexpr const char* fmtSpecPayloadInfoReqContentString =  R"--(
        <payload>
        <extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_InsuranceProvider">
            <valueIdentifier>
                <system value="http://fhir.de/sid/arge-ik/iknr" />
                <value value="109500969" />
            </valueIdentifier>
        </extension>
        <extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_SubstitutionAllowedType">
            <valueBoolean value="false" />
        </extension>
        <extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PrescriptionType">
            <valueCoding>
                <system value="https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType" />
                <code value="160" />
            </valueCoding>
        </extension>
        <extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PackageQuantity">
            <valueQuantity>
                <value value="1" />
                <system value="http://unitsofmeasure.org" />
            </valueQuantity>
        </extension>
        <extension url="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_SupplyOptionsType">
            <extension url="onPremise">
                <valueBoolean value="true" />
            </extension>
            <extension url="shipment">
                <valueBoolean value="false" />
            </extension>
            <extension url="delivery">
                <valueBoolean value="true" />
            </extension>
        </extension>
        <contentString value="%1%" />
    </payload>
    )--";
    std::string body = R"(<Communication xmlns="http://hl7.org/fhir">)";

    const bool isChargeItemRelated =
        mMessageType == model::Communication::MessageType::ChargChangeReq || mMessageType == model::Communication::MessageType::ChargChangeReply;

    body += boost::str(boost::format(fmtSpecProfile) % ::testutils::profile(schemaType, fhirBundleVersion));

    if (mAbout)
    {
        ResourceTemplates::MedicationOptions medicationOptions;
        if (mMedicationOptions)
        {
            medicationOptions = *mMedicationOptions;
        }
        else
        {
            std::string id{*mAbout};
            if (! id.empty() && id[0] == '#')
            {
                id = id.substr(1);
            }
            medicationOptions.version = get<ResourceVersion::KbvItaErp>(profileVersions);
            medicationOptions.id = id;
        }
        body += "<contained>" + ResourceTemplates::medicationXml(medicationOptions) + "</contained>";
    }
    if (mPrescriptionId.has_value() && mAccessCode.has_value())
        body += boost::str(boost::format(fmtSpecBasedOnTaskIdAccessCode) % mPrescriptionId.value() % mAccessCode.value());
    else if (mPrescriptionId.has_value())
        body += boost::str(boost::format(isChargeItemRelated ? fmtSpecBasedOnChargeItemId : fmtSpecBasedOnTaskId) % mPrescriptionId.value());
    body += R"(<status value="unknown"/>)";
    if (mAbout.has_value())
        body += boost::str(boost::format(fmtSpecAbout) % mAbout.value());
    if (mSender.has_value())
        body += boost::str(boost::format(fmtSpecSender)
            % actorRoleToResourceId(std::get<0>(mSender.value()), mProfileVersion)
            % std::get<1>(mSender.value()));
    if (mRecipient.has_value())
        body += boost::str(boost::format(fmtSpecRecipient)
            % actorRoleToResourceId(std::get<0>(mRecipient.value()), mProfileVersion)
            % std::get<1>(mRecipient.value()));
    if (mTimeSent.has_value())
        body += boost::str(boost::format(fmtSpecSent) % mTimeSent.value());
    if (mTimeReceived.has_value())
        body += boost::str(boost::format(fmtSpecReceived) % mTimeReceived.value());
    if (mPayload.has_value())
    {
        auto payload = String::replaceAll(mPayload.value(), "\"", "&quot;");
        if (ResourceVersion::deprecatedProfile(mProfileVersion))
            body += boost::str(boost::format(fmtSpecPayloadContentStringDeprecated) % payload);
        else if (mMessageType == Communication::MessageType::InfoReq)
            body += boost::str(boost::format(fmtSpecPayloadInfoReqContentString) % payload);
        else
            body += boost::str(boost::format(fmtSpecPayloadContentString) % payload);
    }
    body += "</Communication>";
    return body;
}
