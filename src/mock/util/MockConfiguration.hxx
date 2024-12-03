/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKCONFIGURATION_HXX
#define ERP_PROCESSING_CONTEXT_MOCKCONFIGURATION_HXX


#include "shared/util/Configuration.hxx"

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
    MOCK_ID_FD_SIG_SIGNER_CERT,
    MOCK_ID_FD_SIG_OCSP_URL,
    MOCK_ID_FD_SIG_CERT,
    MOCK_USE_MOCK_TPM,
    MOCK_GENERATED_PKI_PATH,
    MOCK_VAU_AUT_PRIVATE_KEY,
    MOCK_VAU_AUT_CERT,
    MOCK_VAU_AUT_SIGNER_CERT
};

class MockConfigurationKeyNames : public ConfigurationKeyNamesBase<MockConfigurationKey>
{
public:
    MockConfigurationKeyNames();
};

using MockConfiguration = ConfigurationTemplate<MockConfigurationKey, MockConfigurationKeyNames>;

#endif
