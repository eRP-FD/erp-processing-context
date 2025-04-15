/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/SensitiveDataGuard.hxx"

#include "shared/util/String.hxx"

namespace epa
{

SensitiveDataGuard& SensitiveDataGuard::operator=(const BinaryBuffer& sensitiveData) noexcept
{
    if (&mSensitiveData != &sensitiveData)
    {
        cleanse();
        mSensitiveData = sensitiveData;
    }
    return *this;
}


// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
SensitiveDataGuard& SensitiveDataGuard::operator=(BinaryBuffer&& sensitiveData) noexcept
{
    if (&mSensitiveData != &sensitiveData)
    {
        moveStringWithCleanse(std::move(sensitiveData.mBytes), mSensitiveData.mBytes);
    }
    return *this;
}


SensitiveDataGuard& SensitiveDataGuard::operator=(const SensitiveDataGuard& other)
{
    if (this != &other)
    {
        cleanse();
        mSensitiveData = other.mSensitiveData;
    }
    return *this;
}


SensitiveDataGuard& SensitiveDataGuard::operator=(SensitiveDataGuard&& other) noexcept
{
    if (this != &other)
    {
        moveStringWithCleanse(std::move(other.mSensitiveData.mBytes), mSensitiveData.mBytes);
    }
    return *this;
}


void SensitiveDataGuard::cleanse()
{
    String::safeClear(mSensitiveData.mBytes);
}


void SensitiveDataGuard::moveStringWithCleanse(std::string&& source, std::string& target)
{
    String::safeClear(target);

    // If std::string::data() points to an area inside the string, it uses the short string
    // optimization.
    const auto* memoryRangeBegin = reinterpret_cast<const std::uint8_t*>(&source);
    // NOLINTNEXTLINE(bugprone-sizeof-container)
    const auto* memoryRangeEnd = memoryRangeBegin + sizeof(source);
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(source.data());
    const bool usesShortStringOptimization = ptr >= memoryRangeBegin && ptr < memoryRangeEnd;

    if (usesShortStringOptimization)
    {
        // If the source std::string uses the SSO, then "moving" it into our string
        // will have to make a copy of the source bytes instead of just adjusting pointers.
        // But then who will securely overwrite the source bytes?
        // Instead, make a copy (which is O(1) from SSO to SSO) and then overwrite the source
        // BinaryBuffer/std::string.
        target = source;
        String::safeClear(source);
    }
    else
    {
        target = std::move(source);
    }
}

} // namespace epa
