/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TESTCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_TESTCONFIGURATION_HXX


#include "erp/util/Configuration.hxx"

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
};

class TestConfigKeyNames : public ConfigurationKeyNamesBase<TestConfigurationKey>
{
public:
    TestConfigKeyNames();
};

using TestConfiguration = ConfigurationTemplate<TestConfigurationKey, TestConfigKeyNames>;


#endif
