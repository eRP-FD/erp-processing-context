/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "JsonTestUtils.hxx"

#include "erp/model/ResourceNames.hxx"

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
    { ActorRole::Representative, naming_system::gkvKvid10   },
    { ActorRole::Doctor,         naming_system::telematicID },
    { ActorRole::Pharmacists,    naming_system::telematicID }
};

const std::string_view& actorRoleToResourceId(ActorRole actorRole)
{
    return ActorRoleToResourceId.find(actorRole)->second;
}

CommunicationJsonStringBuilder::CommunicationJsonStringBuilder(Communication::MessageType messageType) :
    mMessageType(messageType),
    mPrescriptionId(),
    mAccessCode(),
    mSender(),
    mRecipient(),
    mTimeSent(),
    mTimeReceived(),
    mPayload()
{
}

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

std::string CommunicationJsonStringBuilder::createJsonString() const
{
    static constexpr const char* fmtSpecProfile = R"(
            "meta": {"profile": ["https://gematik.de/fhir/StructureDefinition/ErxCommunication%1%"]})";
    static constexpr const char* fmtSpecBasedOnTaskId = R"(
            "basedOn": [{"reference": "Task/%1%"}])";
    static constexpr const char* fmtSpecBasedOnTaskIdAccessCode = R"(
            "basedOn": [{"reference": "Task/%1%/$accept?ac=%2%"}])";
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
    static constexpr const char* fmtSpecPayloadContentString = R"(
            "payload": [{ "contentString": "%1%" }])";
    std::string body = R"({"resourceType": "Communication",)";
    body += boost::str(boost::format(fmtSpecProfile) % Communication::messageTypeToString(mMessageType));
    if (mPrescriptionId.has_value() && mAccessCode.has_value())
        body += "," + boost::str(boost::format(fmtSpecBasedOnTaskIdAccessCode) % mPrescriptionId.value() % mAccessCode.value());
    else if (mPrescriptionId.has_value())
        body += "," + boost::str(boost::format(fmtSpecBasedOnTaskId) % mPrescriptionId.value());
    body += R"(, "status":"unknown")";
    if (mAbout.has_value())
        body += "," + boost::str(boost::format(fmtSpecAbout) % mAbout.value());
    if (mSender.has_value())
        body += "," + boost::str(boost::format(fmtSpecSender)
            % actorRoleToResourceId(std::get<0>(mSender.value()))
            % std::get<1>(mSender.value()));
    if (mRecipient.has_value())
        body += "," + boost::str(boost::format(fmtSpecRecipient)
            % actorRoleToResourceId(std::get<0>(mRecipient.value()))
            % std::get<1>(mRecipient.value()));
    if (mTimeSent.has_value())
        body += "," + boost::str(boost::format(fmtSpecSent) % mTimeSent.value());
    if (mTimeReceived.has_value())
        body += "," + boost::str(boost::format(fmtSpecReceived) % mTimeReceived.value());
    if (mPayload.has_value())
        body += "," + boost::str(boost::format(fmtSpecPayloadContentString) % mPayload.value());
    body += "}";
    return body;
}

std::string CommunicationJsonStringBuilder::createXmlString() const
{
    static constexpr const char* fmtSpecProfile = R"(
            <meta> <profile value="https://gematik.de/fhir/StructureDefinition/ErxCommunication%1%" /></meta>)";
    static constexpr const char* fmtSpecBasedOnTaskId = R"(
            <basedOn> <reference value="Task/%1%"/> </basedOn>)";
    static constexpr const char* fmtSpecBasedOnTaskIdAccessCode = R"(
            <basedOn> <reference value="Task/%1%/$accept?ac=%2%"/> </basedOn>)";
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
    static constexpr const char* fmtSpecPayloadContentString = R"(
            <payload> <contentString value="%1%"/> </payload>)";
    std::string body = R"(<Communication xmlns="http://hl7.org/fhir">)";
    body += boost::str(boost::format(fmtSpecProfile) % Communication::messageTypeToString(mMessageType));
    if (mPrescriptionId.has_value() && mAccessCode.has_value())
        body += boost::str(boost::format(fmtSpecBasedOnTaskIdAccessCode) % mPrescriptionId.value() % mAccessCode.value());
    else if (mPrescriptionId.has_value())
        body += boost::str(boost::format(fmtSpecBasedOnTaskId) % mPrescriptionId.value());
    body += R"(<status value="unknown"/>)";
    if (mAbout.has_value())
        body += boost::str(boost::format(fmtSpecAbout) % mAbout.value());
    if (mSender.has_value())
        body += boost::str(boost::format(fmtSpecSender)
            % actorRoleToResourceId(std::get<0>(mSender.value()))
            % std::get<1>(mSender.value()));
    if (mRecipient.has_value())
        body += boost::str(boost::format(fmtSpecRecipient)
            % actorRoleToResourceId(std::get<0>(mRecipient.value()))
            % std::get<1>(mRecipient.value()));
    if (mTimeSent.has_value())
        body += boost::str(boost::format(fmtSpecSent) % mTimeSent.value());
    if (mTimeReceived.has_value())
        body += boost::str(boost::format(fmtSpecReceived) % mTimeReceived.value());
    if (mPayload.has_value())
        body += boost::str(boost::format(fmtSpecPayloadContentString) % mPayload.value());
    body += "</Communication>";
    return body;
}

