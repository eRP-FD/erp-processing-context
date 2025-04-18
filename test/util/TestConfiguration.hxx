/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TESTCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_TESTCONFIGURATION_HXX


#include "shared/util/Configuration.hxx"

enum class TestConfigurationKey
{
    TEST_ADDITIONAL_XML_SCHEMAS,
    TEST_CADESBES_TRUSTED_CERT_DIR,
    TEST_USE_POSTGRES,
    TEST_USE_REDIS_MOCK,
    TEST_USE_IDP_UPDATER_MOCK,
    TEST_ENABLE_CLIENT_AUTHENTICATON,
    TEST_CLIENT_CERTIFICATE,
    TEST_CLIENT_PRIVATE_KEY,
    TEST_QES_PEM_FILE_NAME,
    TEST_RESOURCE_MANAGER_PATH,
    TEST_VSDM_KEYS,
    TEST_ECIES_CERTIFICATE,

};

class TestConfigKeyNames : public ConfigurationKeyNamesBase<TestConfigurationKey>
{
public:
    TestConfigKeyNames();
};

using TestConfiguration = ConfigurationTemplate<TestConfigurationKey, TestConfigKeyNames>;


#endif
