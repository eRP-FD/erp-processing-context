/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#undef Expect
#include "exporter/client/push/PushGatewayClient.hxx"
#include "shared/network/client/response/ClientResponse.hxx"

#include <gmock/gmock.h>

class PushNotificationClientMock : public PushGatewayClient
{
public:
    MOCK_METHOD(ClientResponse, send,
                (const UrlHelper::UrlParts& url, HttpMethod method, const std::string& body,
                 const std::string& contentType),
                (const, override));
    MOCK_METHOD(ClientResponse, sendPushNotification,
                (const model::EncryptedNotification& encryptedNotification, const std::string& baseUrl), (override));
};
