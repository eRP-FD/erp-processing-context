/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_SIGNATUREVERIFIER_HXX
#define EPA_LIBRARY_CRYPTO_SIGNATUREVERIFIER_HXX

#include <boost/noncopyable.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

/**
 * Helper class for verifying a cryptographic signature.
 * After creating the object, update() can be called repeatedly to provide the signed data.
 * After all data have been passed via calls to update(), verify() can be called to verify the
 * signature.
 */
class SignatureVerifier : boost::noncopyable
{
public:
    /**
     * @param messageDigest The message digest algorithm for hashing the data to be signed, e.g.
     *        EVP_sha256().
     */
    explicit SignatureVerifier(const EVP_MD* messageDigest);

    void update(const void* data, size_t size);
    void update(const std::string_view& data);

    /**
     * Verify the signature after all signed data have been passed via calls to update().
     *
     * @param signature The signature to be verified (binary).
     * @param signatureKey The public key for verifying the signature.
     * @return true if the the signature matches the data, false otherwise.
     */
    bool verify(std::string_view signature, EVP_PKEY* signatureKey);
    bool verify(std::string_view signature, shared_EVP_PKEY signatureKey);

private:
    shared_EVP_MD_CTX mDigestContext;
};

} // namespace epa

#endif
