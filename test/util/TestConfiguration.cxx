/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/TestConfiguration.hxx"

TestConfigKeyNames::TestConfigKeyNames()
{
    using Flags = KeyData::ConfigurationKeyFlags;
    // clang-format off
    mNamesByKey.insert(
        {{TestConfigurationKey::TEST_USE_POSTGRES,                {"TEST_USE_POSTGRES",                "/test/use-postgres", Flags::none, "If disabled, will use database mocks"}},
         {TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS,      {"TEST_ADDITIONAL_XML_SCHEMAS",      "/test/additional-xml-schema", Flags::none, "Load additional XSD schemas"}},
         {TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR,   {"TEST_CADESBES_TRUSTED_CERT_DIR",   "/test/cades-trusted-cert-dir", Flags::none, "Directory for trusted certificates for cades signature validation"}},
         {TestConfigurationKey::TEST_USE_REDIS_MOCK,              {"TEST_USE_REDIS_MOCK",              "/test/use-redis-mock", Flags::none, "Enable redis mock"}},
         {TestConfigurationKey::TEST_USE_IDP_UPDATER_MOCK,        {"TEST_USE_IDP_UPDATER_MOCK",        "/test/use-idp-updater-mock", Flags::none, "Enable IdP update mock"}},
         {TestConfigurationKey::TEST_ENABLE_CLIENT_AUTHENTICATON, {"TEST_ENABLE_CLIENT_AUTHENTICATON", "/test/client/enableAuthentication", Flags::none, "Enable mTLS authentication"}},
         {TestConfigurationKey::TEST_CLIENT_CERTIFICATE,          {"TEST_CLIENT_CERTIFICATE",          "/test/client/certificate", Flags::none, "TLS client certificate"}},
         {TestConfigurationKey::TEST_CLIENT_PRIVATE_KEY,          {"TEST_CLIENT_PRIVATE_KEY",          "/test/client/certificateKey", Flags::none, "TLS client certificate key"}},
         {TestConfigurationKey::TEST_QES_PEM_FILE_NAME,           {"TEST_QES_PEM_FILE_NAME",           "/test/qes-pem-file-name", Flags::none, "PEM file with certificate and key pair used for cades signing."}},
         {TestConfigurationKey::TEST_RESOURCE_MANAGER_PATH,       {"TEST_RESOURCE_MANAGER_PATH",       "/test/resource-manager-path", Flags::none, "Path to test resources"}},
         {TestConfigurationKey::TEST_VSDM_KEYS,                   {"TEST_VSDM_KEYS",                   "/test/vsdm-keys", Flags::none, "VSDM keys added to the database on startup"}},
         {TestConfigurationKey::TEST_ECIES_CERTIFICATE,           {"TEST_ECIES_CERTIFICATE",           "/test/ecies-certificate", Flags::none, "Client ecies certificate."}},
        });
    // clang-format on
}
