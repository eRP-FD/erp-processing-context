/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/push/Channels.hxx"
#include "shared/model/ModelException.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gsl/gsl-lite.hpp>
#include <rapidjson/prettywriter.h>
#include <pqxx/array>
#include <set>
#include <utility>

namespace
{
const rapidjson::Value& getMember(const rapidjson::Value& json, const std::string& memberName)
{
    const auto it = json.FindMember(memberName);
    Expect3(it != json.MemberEnd(), "member " + std::string{memberName} + " is missing",
            model::MissingParameterException);
    return it->value;
}
}

namespace model
{

model::ChannelId toChannelId(std::string s)
{
    auto transformedStr = String::replaceAll(std::string{s}, ".", "_");
    auto val = magic_enum::enum_cast<model::ChannelId>(transformedStr);
    Expect3(val, "ChannelId " + std::string{s} + " is unknown", model::InvalidParameterException);
    return *val;
}

std::string to_string(model::ChannelId chId)
{
    auto s = magic_enum::enum_name(chId);
    auto str = String::replaceAll(std::string{s}, "_", ".");
    return str;
}

void Channels::processUpdate(std::string_view json)
{
    try
    {
        rapidjson::Document document;
        document.Parse(rapidjson::StringRef(json.data(), json.size()));
        Expect3(! document.HasParseError(), "Could not parse Channels from json", model::InvalidParameterException);
        processUpdate(document);
    }
    catch (const ModelException&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        ModelFail(ex.what());
    }
}

void Channels::add(ChannelId channelId)
{
    mActiveChannels.emplace(channelId);
}

std::string Channels::pqxxArrayStr() const
{
    auto channelId = std::views::transform([](const ChannelId channelId)
    {
        return to_string(channelId);
    });

    std::vector<std::string> strs;
    std::ranges::copy(mActiveChannels | channelId, std::back_inserter(strs));
    std::ranges::sort(strs);

    return fmt::format("{{{}}}", fmt::join(strs, ","));
}

std::string Channels::serializeToJsonString() const
{
    static constexpr auto allChannelsArr = magic_enum::enum_values<model::ChannelId>();
    std::vector<model::ChannelId> allChannels{allChannelsArr.begin(), allChannelsArr.end()};

    rapidjson::Document doc;
    doc.SetObject();
    auto channelArray = rapidjson::Value{rapidjson::kArrayType};
    channelArray.Reserve(gsl::narrow<rapidjson::SizeType>(allChannels.size()), doc.GetAllocator());
    for (const auto& channelId : mActiveChannels)
    {
        rapidjson::Value channelJson(rapidjson::kObjectType);
        std::string s = to_string(channelId);
        channelJson.AddMember("id", s, doc.GetAllocator());
        channelJson.AddMember("status", rapidjson::StringRef("enabled"), doc.GetAllocator());
        channelArray.PushBack(channelJson, doc.GetAllocator());
        std::erase(allChannels, channelId);
    }
    for (const auto& inactiveChannel : allChannels)
    {
        rapidjson::Value channelJson(rapidjson::kObjectType);

        channelJson.AddMember("id", to_string(inactiveChannel), doc.GetAllocator());
        channelJson.AddMember("status", rapidjson::StringRef("disabled"), doc.GetAllocator());
        channelArray.PushBack(channelJson, doc.GetAllocator());
    }
    doc.AddMember("channels", channelArray, doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

const std::set<ChannelId>& Channels::activeChannels() const
{
    return mActiveChannels;
}

void Channels::processUpdate(const rapidjson::Value& json)
{
    const auto channels = json.FindMember("channels");
    Expect3(channels != json.MemberEnd(), "member channels is missing", model::MissingParameterException);
    Expect3(channels->value.IsArray(), "member channels is not an array", model::InvalidParameterException);
    for (const auto& channel : channels->value.GetArray())
    {
        const auto& statusJson = getMember(channel, "status");
        Expect3(statusJson.IsString(), "member status must be string", model::InvalidParameterException);
        auto status = magic_enum::enum_cast<model::ChannelStatus>(statusJson.GetString());
        Expect3(status.has_value(), fmt::format("member status is not a valid enum value {}", statusJson.GetString()),
                model::InvalidParameterException);
        const auto& channelId = getMember(channel, "id");
        switch (*status)
        {
            case ChannelStatus::enabled:
                mActiveChannels.emplace(toChannelId(channelId.GetString()));
                break;
            case ChannelStatus::disabled:
                mActiveChannels.erase(toChannelId(channelId.GetString()));
                break;
            case ChannelStatus::not_set:
                break;
        }
    }
}

}// model
