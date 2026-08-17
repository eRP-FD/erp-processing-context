/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/KeyDerivationUtils.hxx"
#include "shared/crypto/OpenSsl.hxx"

#include <memory>

#include "shared/util/Expect.hxx"
#include "shared/util/Hash.hxx"


namespace
{
    using EvpPkeyContextPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

    /**
     * Throws an exception unless the given parameter has the expected value of 1.
     *
     * Will be refactored via the introduction of more specific exception types.
     */
    void throwException(int operationResult)
    {
        // Keep the flow (throwing std::runtime_error) but log the problem with the macro
        OpenSslExpect(1 == operationResult, "KDF error");
    }
}

// GEMREQ-start A_27158
SensitiveDataGuard KeyDerivationUtils::performHkdfHmacSha256(const SensitiveDataGuard& key,
                                                             std::size_t derivationLength,
                                                             const SensitiveDataGuard& info,
                                                             const SensitiveDataGuard& salt)
{
    EvpPkeyContextPtr evpPkeyContext{EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), &EVP_PKEY_CTX_free};
    throwException(nullptr != evpPkeyContext);

    throwException(EVP_PKEY_derive_init(evpPkeyContext.get()));

    throwException(EVP_PKEY_CTX_hkdf_mode(evpPkeyContext.get(), EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND));
    throwException(EVP_PKEY_CTX_set_hkdf_md(evpPkeyContext.get(), EVP_sha256()));
    throwException(EVP_PKEY_CTX_set1_hkdf_salt(evpPkeyContext.get(), salt.data(), salt.sizeAsInt()));
    throwException(EVP_PKEY_CTX_add1_hkdf_info(evpPkeyContext.get(), info.data(), info.sizeAsInt()));
    throwException(EVP_PKEY_CTX_set1_hkdf_key(evpPkeyContext.get(), key.data(), key.sizeAsInt()));

    SensitiveDataGuard derivationResult{BinaryBuffer(derivationLength)};
    auto derivationResultLength{derivationResult.size()};

    throwException(EVP_PKEY_derive(evpPkeyContext.get(), derivationResult.data(), &derivationResultLength));

    return derivationResult;
}
// GEMREQ-end A_27158
