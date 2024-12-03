/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/util/MockConfiguration.hxx"

#include "shared/util/Expect.hxx"

MockConfigurationKeyNames::MockConfigurationKeyNames()
{
    using Flags = KeyData::ConfigurationKeyFlags;
    // clang-format off
    mNamesByKey.insert({
        { MockConfigurationKey::MOCK_ECIES_PUBLIC_KEY,       {"MOCK_ECIES_PUBLIC_KEY",       "/mock/ecies/public-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_ECIES_PRIVATE_KEY,      {"MOCK_ECIES_PRIVATE_KEY",      "/mock/ecies/private-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_IDP_PUBLIC_KEY,         {"MOCK_IDP_PUBLIC_KEY",         "/mock/idp/public-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_IDP_PRIVATE_KEY,        {"MOCK_IDP_PRIVATE_KEY",        "/mock/idp/private-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_PRIVATE_KEY,  {"MOCK_ID_FD_SIG_PRIVATE_KEY",  "/mock/id.fd.sig/private-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_SIGNER_CERT,  {"MOCK_ID_FD_SIG_SIGNER_CERT",  "/mock/id.fd.sig/signer-cert", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_OCSP_URL,     {"MOCK_ID_FD_SIG_OCSP_URL",     "/mock/id.fd.sig/ocsp-url", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_ID_FD_SIG_CERT,         {"MOCK_ID_FD_SIG_CERT",         "/mock/id.fd.sig/cert", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_USE_MOCK_TPM,           {"MOCK_USE_MOCK_TPM",           "/mock/use-mock-tpm", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_GENERATED_PKI_PATH,     {"MOCK_GENERATED_PKI_PATH",     "/mock/generated-pki-path", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_VAU_AUT_PRIVATE_KEY,    {"MOCK_VAU_AUT_PRIVATE_KEY",    "/mock/vau.aut/private-key", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_VAU_AUT_CERT,           {"MOCK_VAU_AUT_CERT",           "/mock/vau.aut/cert", Flags::categoryDebug, ""} },
        { MockConfigurationKey::MOCK_VAU_AUT_SIGNER_CERT,    {"MOCK_VAU_AUT_PRIVATE_KEY",    "/mock/vau.aut/signer-cert", Flags::categoryDebug, ""} }
    });
    // clang-format on
}
