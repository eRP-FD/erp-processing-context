/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "exporter/VauAutTokenSigner.hxx"

#include <rapidjson/rapidjson.h>
#include <set>
#include <string>
#include <vector>

namespace model
{

enum class ChannelId : uint8_t
{
    erp_task_activate          = 0,
    erp_task_accept            = 1,
    erp_task_reject            = 2,
    erp_task_close             = 3,

    erp_task_dispense          = 4,
    erp_task_abort             = 5,
    erp_communication_new      = 6,
    erp_task_vertreter         = 7,

    erp_chargeitem_create      = 8,
    erp_chargeitem_update      = 9,
    erp_eu_prescription_get    = 10,
    erp_eu_prescription_redeem = 11,

    erp_eu_prescription_close  = 12,

    unused                     = UINT8_MAX,
};

ChannelId toChannelId(std::string s);
std::string to_string(ChannelId chId);

enum class ChannelStatus
{
    enabled,
    disabled,
    not_set
};

// https://github.com/gematik/gem-push-notifications-concept/blob/1.2/docs_sources/definitions/channels_post.yaml
class Channels
{
public:
    Channels() = default;

    void processUpdate(std::string_view json);
    void add(ChannelId channelId);

    [[nodiscard]] std::string pqxxArrayStr() const;
    [[nodiscard]] std::string serializeToJsonString() const;

    const std::set<ChannelId>& activeChannels() const;

private:
    void processUpdate(const rapidjson::Value& json);
    std::set<ChannelId> mActiveChannels;
};

}// model
