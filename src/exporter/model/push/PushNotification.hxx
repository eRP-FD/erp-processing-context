/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/ErpRequirements.hxx"

#include <string>

namespace model
{

class PushNotification
{
public:
    static constexpr bool implements = A_28124.implements("Datenstruktur Nachrichteninhalte");

    PushNotification(std::string channelId, std::string identifier, std::string identifierType);

    [[nodiscard]] std::string serializeToJsonString() const;

    [[nodiscard]] std::string channelId() const;
    [[nodiscard]] std::string identifier() const;
    [[nodiscard]] std::string identifierType() const;

private:
    std::string mChannelId;
    std::string mIdentifier;
    std::string mIdentifierType;
};

}// model
