/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/TestConfiguration.hxx"


template<>
std::map<TestConfigurationKey,KeyNames> ConfigurationKeyNamesTemplate<TestConfigurationKey>::mNamesByKey = {
    { TestConfigurationKey::TEST_USE_POSTGRES,                {"TEST_USE_POSTGRES",                "/test/use-postgres"} },
    { TestConfigurationKey::TEST_ADDITIONAL_JSON_SCHEMAS,     {"TEST_ADDITIONAL_JSON_SCHEMAS",     "/test/additional-json-schema"} },
    { TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS,      {"TEST_ADDITIONAL_XML_SCHEMAS",      "/test/additional-xml-schema" } },
    { TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR,   {"TEST_CADESBES_TRUSTED_CERT_DIR",   "/test/cades-trusted-cert-dir" } },
    { TestConfigurationKey::TEST_USE_REDIS_MOCK,              {"TEST_USE_REDIS_MOCK",              "/test/use-redis-mock"} },
    { TestConfigurationKey::TEST_USE_IDP_UPDATER_MOCK,        {"TEST_USE_REDIS_MOCK",              "/test/use-idp-updater-mock"} },
    { TestConfigurationKey::TEST_ENABLE_CLIENT_AUTHENTICATON, {"TEST_ENABLE_CLIENT_AUTHENTICATON", "/test/client/enableAuthentication"} },
    { TestConfigurationKey::TEST_CLIENT_CERTIFICATE,          {"TEST_CLIENT_CERTIFICATE",          "/test/client/certificate"} },
    { TestConfigurationKey::TEST_CLIENT_PRIVATE_KEY,          {"TEST_CLIENT_PRIVATE_KEY",          "/test/client/certificateKey"}},
    { TestConfigurationKey::TEST_QES_PEM_FILE_NAME,           {"TEST_QES_PEM_FILE_NAME",           "/test/qes-pem-file-name"}}};
