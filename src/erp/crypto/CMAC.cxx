/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "CMAC.hxx"

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/OpenSsl.hxx"
#include "erp/util/SafeString.hxx"

std::string CmacSignature::hex() const
{
    return ByteHelper::toHex(gsl::span<const char>(reinterpret_cast<const char*>(data()), size()));
}

CmacSignature CmacSignature::fromBin(const std::string_view& binString)
{
    CmacSignature sig{};
    Expect(binString.size() == sig.size(), "Wrong CMAC-Signature size");
    std::copy(binString.cbegin(), binString.cend(), sig.mSignature.data());
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
    std::copy(binString.cbegin(), binString.cend(), cmac.mKey.data());
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
