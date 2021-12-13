/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/TestConfiguration.hxx"

TestConfigKeyNames::TestConfigKeyNames()
{
    // clang-format off
    mNamesByKey.insert(
        {{TestConfigurationKey::TEST_USE_POSTGRES,                {"TEST_USE_POSTGRES",                "/test/use-postgres"}},
         {TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS_GEMATIK,{"TEST_ADDITIONAL_XML_SCHEMAS_GEMATIK","/test/additional-xml-schema/de.gematik.erezept-workflow.r4"}},
         {TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS_KBV,  {"TEST_ADDITIONAL_XML_SCHEMAS_KBV",  "/test/additional-xml-schema/kbv.ita.erp"}},
         {TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR,   {"TEST_CADESBES_TRUSTED_CERT_DIR",   "/test/cades-trusted-cert-dir" }},
         {TestConfigurationKey::TEST_USE_REDIS_MOCK,              {"TEST_USE_REDIS_MOCK",              "/test/use-redis-mock"}},
         {TestConfigurationKey::TEST_USE_IDP_UPDATER_MOCK,        {"TEST_USE_REDIS_MOCK",              "/test/use-idp-updater-mock"}},
         {TestConfigurationKey::TEST_ENABLE_CLIENT_AUTHENTICATON, {"TEST_ENABLE_CLIENT_AUTHENTICATON", "/test/client/enableAuthentication"}},
         {TestConfigurationKey::TEST_CLIENT_CERTIFICATE,          {"TEST_CLIENT_CERTIFICATE",          "/test/client/certificate"}},
         {TestConfigurationKey::TEST_CLIENT_PRIVATE_KEY,          {"TEST_CLIENT_PRIVATE_KEY",          "/test/client/certificateKey"}},
         {TestConfigurationKey::TEST_QES_PEM_FILE_NAME,           {"TEST_QES_PEM_FILE_NAME",           "/test/qes-pem-file-name"}}});
    // clang-format on
}
