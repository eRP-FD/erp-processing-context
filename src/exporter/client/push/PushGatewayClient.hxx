/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/network/message/MimeType.hxx"
#include "shared/util/UrlHelper.hxx"

#include <memory>
#include <optional>
#include <string>

namespace model
{
class EncryptedNotification;
}
class ClientResponse;
class CrlProvider;
enum class HttpMethod;


class PushGatewayClient
{
public:
    static std::unique_ptr<PushGatewayClient> create(std::shared_ptr<CrlProvider> crlProvider);

    virtual ~PushGatewayClient();

    virtual ClientResponse send(const UrlHelper::UrlParts& url, HttpMethod method, const std::string& body,
                                const std::string& contentType = MimeType::json) const = 0;
    virtual ClientResponse sendPushNotification(const model::EncryptedNotification& encryptedNotification,
                                                const std::string& baseUrl) = 0;
};
