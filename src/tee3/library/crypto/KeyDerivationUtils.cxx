/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/KeyDerivationUtils.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <memory>
#include <stdexcept>

#include "shared/util/Hash.hxx"

namespace epa
{

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
        if (1 != operationResult)
        {
            throw std::runtime_error{"KDF error"};
        }
    }
}


SensitiveDataGuard KeyDerivationUtils::performHkdfHmacSha256(
    const SensitiveDataGuard& key,
    size_t derivationLength)
{
    EvpPkeyContextPtr evpPkeyContext{
        EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), &EVP_PKEY_CTX_free};
    throwException(nullptr != evpPkeyContext);

    throwException(EVP_PKEY_derive_init(evpPkeyContext.get()));

    throwException(
        EVP_PKEY_CTX_hkdf_mode(evpPkeyContext.get(), EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND));
    throwException(EVP_PKEY_CTX_set_hkdf_md(evpPkeyContext.get(), EVP_sha256()));
    throwException(EVP_PKEY_CTX_set1_hkdf_salt(evpPkeyContext.get(), nullptr, 0));
    throwException(EVP_PKEY_CTX_add1_hkdf_info(evpPkeyContext.get(), nullptr, 0));
    throwException(EVP_PKEY_CTX_set1_hkdf_key(evpPkeyContext.get(), key.data(), key.sizeAsInt()));

    SensitiveDataGuard derivationResult{BinaryBuffer(derivationLength)};
    auto derivationResultLength{derivationResult.size()};

    throwException(
        EVP_PKEY_derive(evpPkeyContext.get(), derivationResult.data(), &derivationResultLength));

    return derivationResult;
}

} // namespace epa
