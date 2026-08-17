/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "PushNotification.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"

#include <rapidjson/rapidjson.h>
#include <utility>

namespace model
{

PushNotification::PushNotification(std::string channelId, std::string identifier, std::string identifierType)
    : mChannelId(std::move(channelId))
    , mIdentifier(std::move(identifier))
    , mIdentifierType(std::move(identifierType))
{
}

std::string PushNotification::serializeToJsonString() const
{
    A_28124.start("Push Notifications - Datenstruktur Nachrichteninhalte");
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("ChannelId", rapidjson::StringRef(mChannelId), doc.GetAllocator());
    doc.AddMember("Identifier", rapidjson::StringRef(mIdentifier), doc.GetAllocator());
    doc.AddMember("IdentifierType", rapidjson::StringRef(mIdentifierType), doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

std::string PushNotification::channelId() const
{
    return mChannelId;
}

std::string PushNotification::identifier() const
{
    return mIdentifier;
}

std::string PushNotification::identifierType() const
{
    return mIdentifierType;
}

}// model