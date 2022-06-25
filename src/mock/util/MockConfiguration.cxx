/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "mock/util/MockConfiguration.hxx"

#include "erp/util/Expect.hxx"

MockConfigurationKeyNames::MockConfigurationKeyNames()
{
    // clang-format off
    mNamesByKey.insert({
        { MockConfigurationKey::MOCK_ECIES_PUBLIC_KEY,       {"MOCK_ECIES_PUBLIC_KEY",       "/mock/ecies/public-key"} },
        { MockConfigurationKey::MOCK_ECIES_PRIVATE_KEY,      {"MOCK_ECIES_PRIVATE_KEY",      "/mock/ecies/private-key"} },
        { MockConfigurationKey::MOCK_IDP_PUBLIC_KEY,         {"MOCK_IDP_PUBLIC_KEY",         "/mock/idp/public-key"} },
        { MockConfigurationKey::MOCK_IDP_PRIVATE_KEY,        {"MOCK_IDP_PRIVATE_KEY",        "/mock/idp/private-key"} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_PRIVATE_KEY,  {"MOCK_ID_FD_SIG_PRIVATE_KEY",  "/mock/id.fd.sig/private-key"} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_SIGNER_CERT,  {"MOCK_ID_FD_SIG_SIGNER_CERT",  "/mock/id.fd.sig/signer-cert"} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_OCSP_URL,     {"MOCK_ID_FD_SIG_OCSP_URL",     "/mock/id.fd.sig/ocsp-url"} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_CERT,         {"MOCK_ID_FD_SIG_CERT",         "/mock/id.fd.sig/cert"} },
        { MockConfigurationKey::MOCK_USE_MOCK_TPM,           {"MOCK_USE_MOCK_TPM",           "/mock/use-mock-tpm"} },
        { MockConfigurationKey::MOCK_GENERATED_PKI_PATH,     {"MOCK_GENERATED_PKI_PATH",     "/mock/generated-pki-path"} }
    });
    // clang-format on
}
