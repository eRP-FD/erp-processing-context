/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_CMAC_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_CMAC_HXX

#include "erp/crypto/RandomSource.hxx"

#include <array>
#include <functional>
#include <type_traits>
#include <string>

class CmacKey;
class SafeString;

class CmacSignature
{
public:
    static constexpr size_t SignatureSize = 128 / 8;
    using SignatureType = std::array<uint8_t, SignatureSize>;

    constexpr size_t size() const { return SignatureSize; }
    constexpr const uint8_t* data() const { return mSignature.data(); }

    [[nodiscard]]
    std::string hex() const;

    [[nodiscard]]
    static CmacSignature fromBin(const std::string_view& binString);

    [[nodiscard]]
    static CmacSignature fromHex(const std::string& hexString);

    bool operator == (const CmacSignature&) const;

private:
    SignatureType mSignature;
    friend class CmacKey;
};

class CmacKey
{
public:
    static constexpr size_t KeySize = 128 / 8;
    using KeyType = std::array<uint8_t, KeySize>;

    constexpr size_t size() const { return KeySize; }
    constexpr const uint8_t* data() const { return mKey.data(); }

    [[nodiscard]]
    CmacSignature sign(const std::string_view& message);

    [[nodiscard]]
    static CmacKey fromBin(const std::string_view& binString);

    [[nodiscard]]
    static CmacKey randomKey(RandomSource& randomSource = RandomSource::defaultSource());


    ~CmacKey() noexcept;
    CmacKey(const CmacKey&) = default;
    CmacKey(CmacKey&&) = default;
    CmacKey& operator = (const CmacKey&) = default;
    CmacKey& operator = (CmacKey&&) = default;
private:
    CmacKey() = default;
    KeyType mKey;
};

#endif
