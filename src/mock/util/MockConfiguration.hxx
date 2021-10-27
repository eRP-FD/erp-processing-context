/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_MOCKCONFIGURATION_HXX


#include "erp/util/Configuration.hxx"

#if WITH_HSM_MOCK  != 1
#error MockConfiguration.hxx included but WITH_HSM_MOCK not enabled
#endif
enum class MockConfigurationKey
{
    MOCK_ECIES_PRIVATE_KEY,  // was VAU_PRIVATE_KEY
    MOCK_ECIES_PUBLIC_KEY,   // was VAU_PUBLIC_KEY
    MOCK_IDP_PRIVATE_KEY,
    MOCK_IDP_PUBLIC_KEY,
    MOCK_ID_FD_SIG_PRIVATE_KEY,
    MOCK_USE_BLOB_DATABASE_MOCK,
    MOCK_USE_MOCK_TPM
};

using MockConfigurationKeyNames = ConfigurationKeyNamesTemplate<MockConfigurationKey>;

using MockConfiguration = ConfigurationTemplate<MockConfigurationKey, MockConfigurationKeyNames>;

#endif
