/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/util/SafeString.hxx"

#include <date/date.h>

namespace model
{

class EncryptionKey
{
public:
    static EncryptionKey deriveMonthlyKey(const SafeString& sharedSecret, const std::string& monthInfo);
    static EncryptionKey deriveMonthlyKey(const SafeString& sharedSecret, date::year_month monthCreated);
    static EncryptionKey deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey, const std::string& newMonth);
    static EncryptionKey deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey, date::year_month targetMonth);
    static EncryptionKey deriveFromPreviousMonthlyKey(const EncryptionKey& previousKey);

    EncryptionKey(SafeString sharedSecret, SafeString messageKey, const std::string& timeKeyCreated);
    EncryptionKey(SafeString sharedSecret, SafeString messageKey, date::year_month timeKeyCreated);
    static EncryptionKey deserializeCsv(const SafeString& serializedCsv);
    SafeString serializeToCsv(const std::string& keyIdentifier) const;

    //! Returns the key identifier after csv deserialization, otherwhise returns an empty string.
    [[nodiscard]] std::string getKeyIdentifier() const;

    [[nodiscard]] const SafeString& sharedSecret() const;
    [[nodiscard]] const SafeString& messageKey() const;
    [[nodiscard]] date::year_month monthKeyCreated() const;
    [[nodiscard]] std::string monthInfoString() const;

    [[nodiscard]] bool isUpToDate() const;

    std::string encryptMessage(std::string_view plaintext) const;

    bool operator==(const EncryptionKey& rhs) const = default;

private:
    void setKeyIdentifier(std::string_view keyIdentifier);
    SafeString mSharedSecret;
    SafeString mMessageKey;
    date::year_month mMonthKeyCreated;
    std::string mKeyIdentifier;
};

}// model
