#include "erp/crypto/Sha256.hxx"

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/Buffer.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/OpenSsl.hxx"

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
