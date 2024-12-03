/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/SignatureVerifier.hxx"
#include "library/util/Assert.hxx"
#include "library/util/BinaryBuffer.hxx"

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

SignatureVerifier::SignatureVerifier(const EVP_MD* messageDigest)
  : mDigestContext{shared_EVP_MD_CTX::make()}
{
    AssertOpenSsl(EVP_DigestInit(mDigestContext.get(), messageDigest) == 1);
}


void SignatureVerifier::update(const void* data, size_t size)
{
    AssertOpenSsl(EVP_DigestUpdate(mDigestContext.get(), data, size) == 1);
}


void SignatureVerifier::update(const std::string_view& data)
{
    update(data.data(), data.size());
}


bool SignatureVerifier::verify(std::string_view signature, shared_EVP_PKEY signatureKey)
{
    return verify(signature, signatureKey.get());
}
bool SignatureVerifier::verify(std::string_view signature, EVP_PKEY* signatureKey)
{
    Assert(signatureKey != nullptr);

    // First, finish the streaming "digest"/hashing operation. We did not have the public key when
    // we started the digest, so we have used the EVP_Digest functions so far, not EVP_DigestVerify.
    unsigned digestSize = gsl::narrow<unsigned>(EVP_MD_CTX_get_size(mDigestContext));
    BinaryBuffer digest(digestSize);
    AssertOpenSsl(EVP_DigestFinal(mDigestContext.get(), digest.data(), &digestSize) == 1);
    Assert(digestSize == digest.size());

    // Create a verification context using the given public key.
    auto verificationContext = shared_EVP_PKEY_CTX::make(EVP_PKEY_CTX_new(signatureKey, nullptr));
    AssertOpenSsl(verificationContext != nullptr) << "EVP_PKEY_CTX_new failed";

    // Now verify the signature based on the digest.
    AssertOpenSsl(EVP_PKEY_verify_init(verificationContext) > 0);
    // Note: This does not distinguish between incorrect signatures (return value 0) and
    // invalid/corrupted signatures (return values < 0).
    auto res = EVP_PKEY_verify(
        verificationContext,
        reinterpret_cast<const unsigned char*>(signature.data()),
        signature.size(),
        digest.data(),
        digest.size());
    if (res != 1)
    {
        LOG(WARNING) << "signature verification failed(" << res
                     << "): " << ::epa::assertion::local::getAllOpenSslErrors();
    }
    return res == 1;
}

} // namespace epa
