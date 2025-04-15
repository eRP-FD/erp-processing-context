/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/Sha256.hxx"

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/util/Buffer.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/crypto/OpenSsl.hxx"

std::string Sha256::fromBin(const std::string_view& data)
{
    auto mdContext = shared_EVP_MD_CTX::make();
    Expect3(EVP_DigestInit_ex(mdContext, EVP_sha256(), nullptr) == 1, "Sha256 context creation failed.", std::logic_error);
    Expect3(EVP_DigestUpdate(mdContext, data.data(), data.size()) == 1, "Sha256 update failed.", std::logic_error);
    auto digestSize = gsl::narrow<size_t>(EVP_MD_size(EVP_sha256()));
    Expect3(digestSize > 0 && digestSize < 256, "invalid digest size.", std::logic_error);
    util::Buffer digestBin(digestSize, '\0');
    auto mdSize = gsl::narrow<unsigned>(digestBin.size());
    Expect3(EVP_DigestFinal_ex(mdContext, digestBin.data(), &mdSize) == 1, "Sha256 calculation failed.", std::logic_error);
    Expect3(digestBin.size() == mdSize, "Digest size changed.", std::logic_error);
    return ByteHelper::toHex(gsl::span<const char>(reinterpret_cast<const char*>(digestBin.data()), digestBin.size()));
}
