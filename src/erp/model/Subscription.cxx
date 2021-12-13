/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Subscription.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/RapidjsonDocument.hxx"
#include "erp/util/String.hxx"
#include "erp/util/UrlHelper.hxx"

namespace model
{

namespace
{
// Subscription service is only allowed for Communication requests.
constexpr std::string_view criteria = "Communication";
// Only this HTTP GET argument is supported.
constexpr std::string_view recipient = "recipient";

constexpr std::string_view statusRequested = "requested";
constexpr std::string_view statusActive = "active";
constexpr std::string_view statusUnknown = "unknown";

const rapidjson::Pointer resourceTypePointer("/resourceType");
const rapidjson::Pointer idPointer("/id");
const rapidjson::Pointer statusPointer("/status");
const rapidjson::Pointer endPointer("/end");
const rapidjson::Pointer reasonPointer("/reason");
const rapidjson::Pointer criteriaPointer("/criteria");
const rapidjson::Pointer channelTypePointer("/channel/type");
const rapidjson::Pointer channelHeaderPointer("/channel/header");
}

Subscription::Subscription(NumberAsStringParserDocument&& jsonTree)
    : Resource<Subscription>(std::move(jsonTree))
{
    // Extract recipient id from the GET arguments stored in criteria.
    const auto optionalValue = getOptionalStringValue(criteriaPointer);
    if (optionalValue.has_value())
    {
        const auto [target, query, _] = UrlHelper::splitTarget(std::string{optionalValue.value()});
        if (target == criteria)
        {
            auto pairs = UrlHelper::splitQuery(query);
            for (const auto& pair : pairs)
            {
                if (pair.first == recipient)
                {
                    mRecipient = pair.second;
                    break;
                }
            }
        }
    }
}

const std::string& Subscription::recipientId() const
{
    return mRecipient;
}

Subscription::Status Subscription::status() const
{
    const auto optionalValue = getOptionalStringValue(statusPointer);
    if (! optionalValue.has_value())
    {
        return Status::Unknown;
    }
    if (optionalValue == statusRequested)
    {
        return Status::Requested;
    }
    else if (optionalValue == statusActive)
    {
        return Status::Active;
    }
    return Status::Unknown;
}

void Subscription::setStatus(Status status)
{
    setValue(statusPointer, String::toLower(std::string(magic_enum::enum_name(status))));
}

void Subscription::setSubscriptionId(std::string_view subscriptionId)
{
    setValue(idPointer, subscriptionId);
}

void Subscription::setEndTime(const model::Timestamp& dateTime)
{
    setValue(endPointer, dateTime.toXsDateTimeWithoutFractionalSeconds());
}

void Subscription::setAuthorizationToken(std::string_view jwt)
{
    using namespace std::string_literals;
    addToArray(channelHeaderPointer, "Authorization: Bearer "s.append(jwt));
}

void Subscription::setChannelType(ChannelType channelType)
{
    setValue(channelTypePointer, magic_enum::enum_name(channelType));
}

void Subscription::setReason(std::string_view reason)
{
    setValue(reasonPointer, reason);
}

std::optional<std::string_view> Subscription::reason() const
{
    return getOptionalStringValue(reasonPointer);
}

}
