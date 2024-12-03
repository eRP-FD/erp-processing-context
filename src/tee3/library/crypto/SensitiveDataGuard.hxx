/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_SENSITIVEDATAGUARD_HXX
#define EPA_LIBRARY_CRYPTO_SENSITIVEDATAGUARD_HXX

#include "library/util/BinaryBuffer.hxx"

namespace epa
{

/**
 * Class for holding some data which is cleansed in memory when overwritten or destructed.
 * When constructed or assigned from an rvalue reference (string, BinaryBuffer or
 * SensitiveDataGuard), then the source is also cleansed.
 */
class SensitiveDataGuard
{
public:
    SensitiveDataGuard() = default;

    // NOLINTBEGIN(google-explicit-constructor, hicpp-explicit-conversions)
    explicit(false) SensitiveDataGuard(const BinaryBuffer& sensitiveData);
    explicit(false) SensitiveDataGuard(BinaryBuffer&& sensitiveData) noexcept;
    // NOLINTEND(google-explicit-constructor, hicpp-explicit-conversions)

    explicit SensitiveDataGuard(const char* sensitiveData);
    explicit SensitiveDataGuard(const std::string& sensitiveData);
    explicit SensitiveDataGuard(std::string&& sensitiveData) noexcept;
    SensitiveDataGuard(const void* data, std::size_t size);

    SensitiveDataGuard(const SensitiveDataGuard& other) = default;
    SensitiveDataGuard(SensitiveDataGuard&& other) noexcept;

    ~SensitiveDataGuard();

    SensitiveDataGuard& operator=(const BinaryBuffer& sensitiveData) noexcept;
    SensitiveDataGuard& operator=(BinaryBuffer&& sensitiveData) noexcept;
    SensitiveDataGuard& operator=(const SensitiveDataGuard& other);
    SensitiveDataGuard& operator=(SensitiveDataGuard&& other) noexcept;

    const BinaryBuffer& getBinaryBuffer() const;
    const std::string& getString() const;

    const BinaryBuffer& operator*() const;

    const BinaryBuffer* operator->() const;

    auto operator<=>(const SensitiveDataGuard&) const = default;

    bool empty() const;
    size_t size() const;
    int sizeAsInt() const;
    const std::uint8_t* data() const;
    std::uint8_t* data();

    void cleanse();

private:
    static void moveStringWithCleanse(std::string&& source, std::string& target);

    BinaryBuffer mSensitiveData;
};


inline SensitiveDataGuard::SensitiveDataGuard(const BinaryBuffer& sensitiveData)
  : mSensitiveData(sensitiveData)
{
}


// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
inline SensitiveDataGuard::SensitiveDataGuard(BinaryBuffer&& sensitiveData) noexcept
{
    moveStringWithCleanse(std::move(sensitiveData.mBytes), mSensitiveData.mBytes);
}


inline SensitiveDataGuard::SensitiveDataGuard(const char* sensitiveData)
  : SensitiveDataGuard(std::string(sensitiveData))
{
}


inline SensitiveDataGuard::SensitiveDataGuard(const std::string& sensitiveData)
  : SensitiveDataGuard(BinaryBuffer(sensitiveData))
{
}


inline SensitiveDataGuard::SensitiveDataGuard(std::string&& sensitiveData) noexcept
{
    moveStringWithCleanse(std::move(sensitiveData), mSensitiveData.mBytes);
}


inline SensitiveDataGuard::SensitiveDataGuard(const void* data, std::size_t size)
  : mSensitiveData(data, size)
{
}


// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
inline SensitiveDataGuard::SensitiveDataGuard(SensitiveDataGuard&& other) noexcept
{
    moveStringWithCleanse(std::move(other.mSensitiveData.mBytes), mSensitiveData.mBytes);
}


inline SensitiveDataGuard::~SensitiveDataGuard()
{
    cleanse();
}


inline const BinaryBuffer& SensitiveDataGuard::getBinaryBuffer() const
{
    return mSensitiveData;
}


inline const std::string& SensitiveDataGuard::getString() const
{
    return mSensitiveData.getString();
}


inline const BinaryBuffer& SensitiveDataGuard::operator*() const
{
    return mSensitiveData;
}


inline const BinaryBuffer* SensitiveDataGuard::operator->() const
{
    return &mSensitiveData;
}


inline bool SensitiveDataGuard::empty() const
{
    return mSensitiveData.empty();
}


inline size_t SensitiveDataGuard::size() const
{
    return mSensitiveData.size();
}


inline int SensitiveDataGuard::sizeAsInt() const
{
    return mSensitiveData.sizeAsInt();
}


inline const std::uint8_t* SensitiveDataGuard::data() const
{
    return mSensitiveData.data();
}


inline std::uint8_t* SensitiveDataGuard::data()
{
    return mSensitiveData.data();
}

} // namespace epa

#endif
