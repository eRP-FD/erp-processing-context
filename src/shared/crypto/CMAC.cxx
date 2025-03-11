/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
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
    std::unique_ptr<CMAC_CTX, void (*)(CMAC_CTX*)> ctx{ CMAC_CTX_new(), CMAC_CTX_free};
    Expect(CMAC_Init(ctx.get(), data(), size(), EVP_aes_128_cbc(), nullptr) == 1, "CMAC context initialization failed");
    Expect(CMAC_Update(ctx.get(), message.data(), message.size()) == 1, "CMAC calculation failed");
    CmacSignature sig{};
    auto len = sig.size();
    Expect(CMAC_Final(ctx.get(), sig.mSignature.data(), &len) == 1, "CMAC finalization failed.");
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
