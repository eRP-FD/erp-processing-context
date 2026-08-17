/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/push/EncryptionKey.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/KeyDerivationUtils.hxx"
#include "shared/crypto/RandomSource.hxx"
#include "shared/crypto/SensitiveDataGuard.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"

#include <fmt/format.h>
#include <sstream>


namespace
{

// GEMREQ-start A_27610
std::string padToFixedSize(std::string_view plaintext)
{
    A_27610.start("Obsure the plaintext message size by padding");
    constexpr size_t targetSize = 1024;
    constexpr size_t headerSize = 4;
    constexpr size_t lengthSize = 2;
    constexpr size_t fixedOverhead = headerSize + lengthSize;

    const size_t messageSize = plaintext.size();
    Expect(messageSize + fixedOverhead <= targetSize,
           fmt::format("Message too large for padding: {} bytes", messageSize));

    const size_t paddingSize = targetSize - fixedOverhead - messageSize;

    std::string padded;
    padded.reserve(targetSize);

    padded.append("PNM1", headerSize);

    // Write padding size as big-endian (network byte order)
    padded.push_back(static_cast<char>((paddingSize >> 8) & 0xFF));
    padded.push_back(static_cast<char>(paddingSize & 0xFF));

    padded.append(paddingSize, ' ');

    padded.append(plaintext);
    A_27610.finish();

    return padded;
}
// GEMREQ-end A_27610

date::year_month parseYearMonth(const std::string& yearMonth)
{
    std::istringstream stream(yearMonth);
    date::year_month result{};
    stream.imbue(std::locale::classic());
    stream >> date::parse<date::year_month>("%Y-%m", result);
    ModelExpect(! stream.fail(), "Parsing date failed");
    ModelExpect(stream.get() == std::istringstream::traits_type::eof(), "Parsing date incomplete");
    return result;
}

std::string formatMonthInfoString(date::year_month yearMonth)
{
    return date::format(std::locale::classic(), "%Y-%m", yearMonth);
}

}// namespace

namespace model
{

// GEMREQ-start A_27157
// GEMREQ-start A_27158
EncryptionKey EncryptionKey::deriveMonthlyKey(const SafeString& sharedSecret, const std::string& monthInfo)
{
    A_27157.start("Derive new shared secret and encrpytion key from previous shared secret and new month info");
    ModelExpect(sharedSecret.size() == 32, "Unexpected length for the shared secret");
    A_27158_01.start("Use HKDF-HMAC-SHA256 to derive new key");
    auto derivedKey = KeyDerivationUtils::performHkdfHmacSha256(SensitiveDataGuard{sharedSecret}, 64, SensitiveDataGuard{monthInfo});
    A_27158_01.finish();
    SensitiveDataGuard newSharedSecret{derivedKey.getBinaryBuffer().data(), 32};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    SensitiveDataGuard messageKey{derivedKey.getBinaryBuffer().data() + 32, 32};
    A_27157.finish();
    return {std::move(newSharedSecret).getSafeString(), std::move(messageKey).getSafeString(), monthInfo};
}
// GEMREQ-end A_27158
// GEMREQ-end A_27157


EncryptionKey EncryptionKey::deriveMonthlyKey(const SafeString& sharedSecret, date::year_month monthCreated)
{
    return deriveMonthlyKey(sharedSecret, formatMonthInfoString(monthCreated));
}

EncryptionKey EncryptionKey::deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey, const std::string& newMonth)
{
    return deriveFromPreviousMonthlyKey(previousKey, parseYearMonth(newMonth));
}

// GEMREQ-start A_27160#deriveFromPreviousMonthlyKey
EncryptionKey EncryptionKey::deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey, date::year_month targetMonth)
{
    A_27160.start("Derive the new key based on an old key iteratively for each month");
    if (targetMonth <= previousKey.mMonthKeyCreated)
    {
        return previousKey;
    }

    auto month = previousKey.mMonthKeyCreated;
    auto newKey = previousKey;

    while (month < targetMonth)
    {
        month += date::months{1};
        newKey = deriveMonthlyKey(newKey.sharedSecret(), month);
    }
    A_27160.finish();
    return newKey;
}

EncryptionKey EncryptionKey::deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey)
{
    const date::year_month_day currentYMD{model::Timestamp::now().localDay()};
    return deriveFromPreviousMonthlyKey(previousKey, date::year_month{currentYMD.year(), currentYMD.month()});
}
// GEMREQ-end A_27160#deriveFromPreviousMonthlyKey

EncryptionKey::EncryptionKey(SafeString sharedSecret, SafeString messageKey, const std::string& timeKeyCreated)
    : EncryptionKey(std::move(sharedSecret), std::move(messageKey), parseYearMonth(timeKeyCreated))
{
}

EncryptionKey::EncryptionKey(SafeString sharedSecret, SafeString messageKey, date::year_month timeKeyCreated)
    : mSharedSecret(std::move(sharedSecret))
    , mMessageKey(std::move(messageKey))
    , mMonthKeyCreated(timeKeyCreated)
{
    ModelExpect(mMessageKey.size() == 32, "Encryption key must be 32 bytes");
    ModelExpect(mSharedSecret.size() == 32, "Shared secret must be 32 bytes");
}

EncryptionKey EncryptionKey::deserializeCsv(const SafeString& serializedCsv)
{
    const std::string_view s{serializedCsv};
    const size_t firstComma = s.find_first_of(',');
    const size_t secondComma = s.find_first_of(',', firstComma + 1);
    const size_t thirdComma = s.find(',', secondComma + 1);
    ModelExpect(firstComma != std::string_view::npos && secondComma != std::string_view::npos && thirdComma != std::string_view::npos,
                "Could not parse EncryptionKey from csv: comma not found");
    auto sharedSecret = Base64::decodeToSafeString(s.substr(0, firstComma));
    auto messageKey = Base64::decodeToSafeString(s.substr(firstComma + 1, secondComma - firstComma - 1));
    auto monthKeyCreated = parseYearMonth(std::string{s.substr(secondComma + 1, thirdComma - secondComma - 1)});
    auto keyIdentifierB64 = s.substr(thirdComma + 1);
    auto result = EncryptionKey{std::move(sharedSecret), std::move(messageKey), monthKeyCreated};
    result.setKeyIdentifier(Base64::decodeToSafeString(keyIdentifierB64));
    return result;
}

void EncryptionKey::setKeyIdentifier(std::string_view keyIdentifier)
{
    mKeyIdentifier.assign(keyIdentifier);
}

SafeString EncryptionKey::serializeToCsv(const std::string& keyIdentifier) const
{
    auto sharedSecretB64 = Base64::encodeToSafeString(mSharedSecret);
    auto messageKeyB64 = Base64::encodeToSafeString(mMessageKey);
    auto keyIdentifierB64 = Base64::encode(keyIdentifier);
    std::string timeKeyCreated = formatMonthInfoString(mMonthKeyCreated);
    const auto size = sharedSecretB64.size() + messageKeyB64.size() + timeKeyCreated.size() + keyIdentifierB64.size() + 3; // 3 for the chars ,,,
    SafeString out{SafeString::no_zero_fill, size};
    auto fmtResult =
        fmt::format_to_n(out.c_str(), size, R"({},{},{},{})", sharedSecretB64.c_str(), messageKeyB64.c_str(), timeKeyCreated, keyIdentifierB64);
    ModelExpect(
        fmtResult.size == size,
        fmt::format("Could not format EncryptionKey to csv: calculated size was wrong calculated={} != needed={}", size,
                    fmtResult.size));
    return out;
}

std::string EncryptionKey::getKeyIdentifier() const
{
    return mKeyIdentifier;
}

const SafeString& EncryptionKey::sharedSecret() const
{
    return mSharedSecret;
}

const SafeString& EncryptionKey::messageKey() const
{
    return mMessageKey;
}

date::year_month EncryptionKey::monthKeyCreated() const
{
    return mMonthKeyCreated;
}

std::string EncryptionKey::monthInfoString() const
{
    return formatMonthInfoString(mMonthKeyCreated);
}

// GEMREQ-start A_27160#isuptodate
bool EncryptionKey::isUpToDate() const
{
    const date::year_month_day currentYMD{model::Timestamp::now().localDay()};
    return monthKeyCreated() >= date::year_month{currentYMD.year(), currentYMD.month()};
}
// GEMREQ-end A_27160#isuptodate

std::string EncryptionKey::encryptMessage(std::string_view plaintext) const
{
    A_27161.start("Encrypt push notification");
    // GEMREQ-start A_27161#encryption
    ModelExpect(mMessageKey.size() == 32, "Encryption key must be 32 bytes");

    const std::string paddedPlaintext = padToFixedSize(plaintext);

    const auto iv = RandomSource::defaultSource().randomBytes(AesGcm256::IvLength);

    const auto [ciphertext, authenticationTag] = AesGcm256::encrypt(paddedPlaintext, mMessageKey, iv);

    std::string encryptedData;
    encryptedData.reserve(iv.size() + ciphertext.size() + authenticationTag.size());
    encryptedData.append(iv, iv.size()).append(ciphertext).append(authenticationTag);
    A_27161.finish();

    return Base64::encode(encryptedData);
    // GEMREQ-end A_27161#encryption
}

}// model
