/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SUBSCRIPTION_HXX
#define ERP_PROCESSING_CONTEXT_SUBSCRIPTION_HXX

#include "shared/model/Resource.hxx"

#include <rapidjson/document.h>

namespace model
{
class Timestamp;

// NOLINTNEXTLINE(bugprone-exception-escape)
class Subscription : public Resource<Subscription>
{
public:
    static constexpr auto resourceTypeName = "Subscription";
    static constexpr auto profileType = ProfileType::Subscription;

    enum class Status : std::uint32_t
    {
        Unknown,
        Requested,
        Active
    };

    enum class ChannelType : std::uint32_t
    {
        websocket
    };

    Subscription() = delete;

    //! Get the received recipient id from the request.
    const std::string& recipientId() const;

    //! Get received status as specified in the request.
    Status status() const;

    //! Alter the status in the response.
    void setStatus(Status status);

    //! Unique mapping to receivers Communication objects telematic id.
    void setSubscriptionId(std::string_view subscriptionId);

    //! Set subscription expiration timestamp.
    void setEndTime(const model::Timestamp& dateTime);

    //! Serialized Jwt.
    void setAuthorizationToken(std::string_view jwt);

    void setChannelType(ChannelType channelType);

    void setReason(std::string_view reason);

    std::optional<std::string_view> reason() const;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend class Resource;
    explicit Subscription(NumberAsStringParserDocument&& jsonTree);

    std::string mRecipient;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Subscription>;
}

#endif//ERP_PROCESSING_CONTEXT_SUBSCRIPTION_HXX
