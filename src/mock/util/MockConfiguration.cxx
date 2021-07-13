#include "mock/util/MockConfiguration.hxx"

#include "erp/util/Expect.hxx"

template<>
std::map<MockConfigurationKey,KeyNames> ConfigurationKeyNamesTemplate<MockConfigurationKey>::mNamesByKey = {
    { MockConfigurationKey::MOCK_ECIES_PUBLIC_KEY,       {"MOCK_ECIES_PUBLIC_KEY",       "/mock/ecies/public-key"} },
    { MockConfigurationKey::MOCK_ECIES_PRIVATE_KEY,      {"MOCK_ECIES_PRIVATE_KEY",      "/mock/ecies/private-key"} },
    { MockConfigurationKey::MOCK_IDP_PUBLIC_KEY,         {"MOCK_IDP_PUBLIC_KEY",         "/mock/idp/public-key"} },
    { MockConfigurationKey::MOCK_IDP_PRIVATE_KEY,        {"MOCK_IDP_PRIVATE_KEY",        "/mock/idp/private-key"} },
    { MockConfigurationKey::MOCK_ID_FD_SIG_PRIVATE_KEY,  {"MOCK_ID_FD_SIG_PRIVATE_KEY",  "/mock/id.fd.sig/private-key"} },
    { MockConfigurationKey::MOCK_USE_BLOB_DATABASE_MOCK, {"MOCK_USE_BLOB_DATABASE_MOCK", "/mock/use-blob-database-mock"} },
    { MockConfigurationKey::MOCK_USE_MOCK_TPM,           {"MOCK_USE_MOCK_TPM",           "/mock/use-mock-tpm"} }
};
