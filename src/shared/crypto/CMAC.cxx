/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <algorithm>

#include "CMAC.hxx"

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "shared/crypto/OpenSsl.hxx"
#include "shared/util/SafeString.hxx"

#include <openssl/evp.h>
#include <openssl/core_names.h>

std::string CmacSignature::hex() const
{
    return ByteHelper::toHex(gsl::span<const char>(reinterpret_cast<const char*>(data()), size()));
}

CmacSignature CmacSignature::fromBin(const std::string_view& binString)
{
    CmacSignature sig{};
    Expect(binString.size() == sig.size(), "Wrong CMAC-Signature size");
    std::ranges::copy(binString, sig.mSignature.data());
    return sig;

}

CmacSignature CmacSignature::fromHex(const std::string& hexString)
{
    auto asBin = ByteHelper::fromHex(hexString);
    return fromBin(asBin);
}

bool CmacSignature::operator==(const CmacSignature& other) const
{
    return mSignature == other.mSignature;
}

std::string_view toString(CmacKeyCategory cmacCat)
{
    switch (cmacCat)
    {
        case CmacKeyCategory::user:
            return "Pre-Nutzerpseudonym (PNP)";
        case CmacKeyCategory::telematic:
            return "SubscriptionId";
    }
    Fail("invalid value for CmacKeyCategory:" + std::to_string(static_cast<uintmax_t>(cmacCat)));
}

CmacSignature CmacKey::sign(const std::string_view& message) const
{
    std::unique_ptr<EVP_MAC, decltype(&EVP_MAC_free)> mac{EVP_MAC_fetch(nullptr, "CMAC", nullptr), &EVP_MAC_free};
    std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)> ctx{EVP_MAC_CTX_new(mac.get()), &EVP_MAC_CTX_free};
    OpenSslExpect(ctx != nullptr, "Unable to create MAC context");

    auto aes128cbc = std::to_array("AES-128-CBC");
    const std::array<OSSL_PARAM, 2> params{OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, aes128cbc.data(), 0),
                                           OSSL_PARAM_construct_end()};

    OpenSslExpect(EVP_MAC_init(ctx.get(), data(), size(), params.data()) == 1, "CMAC context initialization failed");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    Expect(EVP_MAC_update(ctx.get(), reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1,
           "CMAC calculation failed");
    CmacSignature sig{};
    auto len = sig.size();
    Expect(EVP_MAC_final(ctx.get(), sig.mSignature.data(), &len, sig.size()) == 1, "CMAC finalization failed.");
    Expect(len == sig.size(), "OpenSSL returned unexpected signature length");
    return sig;
}


CmacKey CmacKey::fromBin(const std::string_view& binString)
{
    CmacKey cmac{};
    Expect(binString.size() == cmac.size(), "Wrong CMAC-Key size");
    std::ranges::copy(binString, cmac.mKey.data());
    return cmac;

}

CmacKey CmacKey::randomKey(RandomSource& randomSource)
{
    return fromBin(randomSource.randomBytes(KeySize));
}

CmacKey::~CmacKey() noexcept
{
    OPENSSL_cleanse(mKey.data(), mKey.size());
}
