/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "JsonTestUtils.hxx"
#include "TestUtils.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceNames.hxx"
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

const std::map<ActorRole, std::string_view> ActorRoleToResourceId = {
    { ActorRole::Insurant,       naming_system::gkvKvid10   },
    { ActorRole::PkvInsurant,    naming_system::pkvKvid10   },
    { ActorRole::Representative, naming_system::gkvKvid10   },
    { ActorRole::Doctor,         naming_system::telematicID },
    { ActorRole::Pharmacists,    naming_system::telematicID }
};

const std::string_view& actorRoleToResourceId(ActorRole actorRole)
{
    return ActorRoleToResourceId.find(actorRole)->second;
}

CommunicationJsonStringBuilder::CommunicationJsonStringBuilder(Communication::MessageType messageType,
                                                               std::optional<fhirtools::FhirVersion> profileVersion)
    : mMessageType(messageType)
    , mPrescriptionId()
    , mAccessCode()
    , mSender()
    , mRecipient()
    , mTimeSent()
    , mTimeReceived()
    , mPayload()
    , mProfileVersion(std::move(profileVersion))
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
    const ProfileType profileType = model::Communication::messageTypeToProfileType(mMessageType);

    static constexpr const char* fmtSpecProfile = R"(
            "meta": {"profile": ["%1%|%2%"]})";
    static constexpr const char fmtFlowTypeExtension[] = R"(
                    {
                        "url": "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PrescriptionType",
                        "valueCoding": {
                            "system": "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType",
                            "code": "%1%"
                        }
                    })";
    static constexpr const char* fmtSpecBasedOnTaskId = R"(
            "basedOn": [{"reference": "Task/%1%"}])";
    static constexpr const char* fmtSpecBasedOnTaskIdAccessCode = R"(
            "basedOn": [{"reference": "Task/%1%/$accept?ac=%2%"}])";
    static constexpr const char* fmtSpecBasedOnChargeItemId = R"(
            "basedOn": [{"reference": "ChargeItem/%1%"}])";
    static constexpr const char* fmtSpecAbout = R"(
            "about": [{"reference": "%1%"}])";
    static constexpr const char* fmtSpecSender = R"(
            "sender": {"identifier": {%3%"system": "%1%","value" : "%2%"}})";
    static constexpr const char* fmtSpecRecipient = R"(
            "recipient": [{"identifier": {%3%"system": "%1%","value" : "%2%"}}])";
    static constexpr const char* fmtSpecSent = R"(
            "sent": "%1%")";
    static constexpr const char* fmtSpecReceived = R"(
            "received": "%1%")";
    static constexpr const char* fmtSpecPayloadContentString = R"--(
            "payload": [
              {
                "contentString": %1%
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
                "contentString": %1%
              }
            ])--";
    std::string body = R"({"resourceType": "Communication",)";

    const bool isChargeItemRelated =
        mMessageType == model::Communication::MessageType::ChargChangeReq || mMessageType == model::Communication::MessageType::ChargChangeReply;
    const auto defaulVersion =
        isChargeItemRelated
            ? static_cast<const fhirtools::FhirVersion&>(ResourceTemplates::Versions::GEM_ERPCHRG_current())
            : static_cast<const fhirtools::FhirVersion&>(ResourceTemplates::Versions::GEM_ERP_current());
    body += boost::str(boost::format(fmtSpecProfile) % profile(profileType).value() %
                       mProfileVersion.value_or(defaulVersion));

    const ResourceTemplates::Versions::GEM_ERP gemVersion{
        mProfileVersion.value_or(ResourceTemplates::Versions::GEM_ERP_current())};

    if (mMessageType == model::Communication::MessageType::DispReq &&
        gemVersion >= ResourceTemplates::Versions::GEM_ERP_1_4)
    {
        Expect3(mPrescriptionId && mPrescriptionId->size() >= 3,
                "Cannot create Communication message: missing prescription id", std::logic_error);
        body += ",\n";
        body += R"(            "extension": [)""\n";
        body += boost::str(boost::format(fmtFlowTypeExtension) % mPrescriptionId->substr(0, 3));
        body += "            ]\n";
    }
    if (mPrescriptionId.has_value() && mAccessCode.has_value())
        body += "," + boost::str(boost::format(fmtSpecBasedOnTaskIdAccessCode) % mPrescriptionId.value() % mAccessCode.value());
    else if (mPrescriptionId.has_value())
        body += "," + boost::str(boost::format(isChargeItemRelated ? fmtSpecBasedOnChargeItemId : fmtSpecBasedOnTaskId) % mPrescriptionId.value());
    body += R"(, "status":"unknown")";
    if (mAbout.has_value())
        body += "," + boost::str(boost::format(fmtSpecAbout) % mAbout.value());
    if (mSender.has_value())
    {
        body += "," + boost::str(boost::format(fmtSpecSender) % actorRoleToResourceId(std::get<0>(mSender.value())) %
                                std::get<1>(mSender.value()) % senderIdentifierType());
    }
    if (mRecipient.has_value())
    {
        body +=
           "," + boost::str(boost::format(fmtSpecRecipient) % actorRoleToResourceId(std::get<0>(mRecipient.value())) %
                            std::get<1>(mRecipient.value()) % receipientIdentifierType());
    }
    if (mTimeSent.has_value())
        body += "," + boost::str(boost::format(fmtSpecSent) % mTimeSent.value());
    if (mTimeReceived.has_value())
        body += "," + boost::str(boost::format(fmtSpecReceived) % mTimeReceived.value());
    if (mPayload.has_value())
    {
        auto payload = std::quoted(mPayload.value());
        if (mMessageType == Communication::MessageType::InfoReq)
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
            if (gemVersion >= ResourceTemplates::Versions::GEM_ERP_1_4)
            {
                medicationOptions.version = gemVersion;
            }
            else
            {
                medicationOptions.version = ResourceTemplates::Versions::KBV_ERP_current(model::Timestamp::now());
            }
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
    return Fhir::instance().converter().jsonToXmlString(
        model::NumberAsStringParserDocument::fromJson(createJsonString()));
}

std::string CommunicationJsonStringBuilder::senderIdentifierType() const
{
    switch (mMessageType)
    {
        case Communication::MessageType::InfoReq:
        case Communication::MessageType::DispReq:
        case Communication::MessageType::Representative:
            if (mProfileVersion.value_or(ResourceTemplates::Versions::GEM_ERP_current()) >=
                ResourceTemplates::Versions::GEM_ERP_1_4)
            {
                return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "KVZ10"}]},)";
            }
            else
            {
                return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "GKV"}]},)";
            }
        case Communication::MessageType::Reply:
        case Communication::MessageType::ChargChangeReply:
            return R"("type":{"coding": [{"code": "PRN", "system": "http://terminology.hl7.org/CodeSystem/v2-0203"}]},)";
        case Communication::MessageType::ChargChangeReq:
            return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "PKV"}]},)";
    }
    return "";
}

std::string CommunicationJsonStringBuilder::receipientIdentifierType() const
{
    switch (mMessageType)
    {
        case Communication::MessageType::InfoReq:
        case Communication::MessageType::DispReq:
        case Communication::MessageType::ChargChangeReq:
            return R"("type":{"coding": [{"code": "PRN", "system": "http://terminology.hl7.org/CodeSystem/v2-0203"}]},)";
        case Communication::MessageType::Reply:
        case Communication::MessageType::Representative:
            if (mProfileVersion.value_or(ResourceTemplates::Versions::GEM_ERP_current()) >=
                ResourceTemplates::Versions::GEM_ERP_1_4)
            {
                return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "KVZ10"}]},)";
            }
            else
            {
                return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "GKV"}]},)";
            }
        case Communication::MessageType::ChargChangeReply:
            return R"("type":{"coding": [{"system": "http://fhir.de/CodeSystem/identifier-type-de-basis", "code": "PKV"}]},)";
    }
    return "";
}
