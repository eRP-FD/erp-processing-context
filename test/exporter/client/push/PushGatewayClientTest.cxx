/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/push/PushGatewayClient.hxx"
#include "shared/network/client/CrlDownloadCache.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>


class PushGatewayClientTest : public testing::Test
{
public:
    static constexpr char requestBody[] =
    R"({
  "notifications": [
    {
      "id": "enc_batch_1",
      "notification": {
        "ciphertext": "base64_of_ciphertext_1_0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF01234",
        "time_message_encrypted": "2024-11",
        "key_identifier": "123e4567-e89b-12d3-a456-426614174000",
        "prio": "high",
        "counts": {
          "unread": 1,
          "missed_calls": 0
        },
        "device": {
          "app_id": "app_id",
          "pushkey": "pushkey",
          "pushkey_ts": 1634025600,
          "data": {
            "format": "format"
          },
          "tweaks": {
            "tweak": "tweak"
          }
        }
      }
    }
  ]
})";
};

TEST_F(PushGatewayClientTest, notifyEncrypted)
{
    if (!TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_PUSH_GATEWAY_CLIENT, false))
    {
        GTEST_SKIP() << "Push gateway unavailable.";
    }
    const auto& configuration = Configuration::instance();
    auto crlRequestSender = std::make_shared<UrlRequestSender>(
        TlsCertificateVerifier::withCustomRootCertificates(""),
        std::chrono::seconds{configuration.getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
        std::chrono::milliseconds{
            configuration.getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});
    crlRequestSender->setProxies(configuration.proxyParameters(ProxyMode::HTTP));

    crlRequestSender->setFollowRedirects(true);
    auto crlProvider = std::make_shared<CrlDownloadCache>(crlRequestSender);
    auto pushGatewayClient = PushGatewayClient::create(crlProvider);
    auto url = UrlHelper::parseUrl("https://localhost:19443/push/v1/notifyEncrypted/batch");
    std::optional<ClientResponse> response;

    ASSERT_NO_THROW(response.emplace(pushGatewayClient->send(url, HttpMethod::POST, requestBody)));
    EXPECT_EQ(response->getHeader().status(), HttpStatus::OK);
}
