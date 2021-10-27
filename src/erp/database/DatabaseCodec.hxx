/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASECODEC_HXX
#define ERP_PROCESSING_CONTEXT_DATABASECODEC_HXX

#include <functional>
#include <type_traits>

#include "erp/compression/Compression.hxx"
#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"

namespace db_model
{
class EncryptedBlob;
}

class DataBaseCodec
{
public:
    using EncryptionT = AesGcm256;
    static constexpr uint8_t version = 1u;
    static constexpr size_t versionLength = sizeof(uint8_t);
    static constexpr size_t minEncodedLength =
        EncryptionT::IvLength + EncryptionT::AuthenticationTagLength + versionLength;

    explicit DataBaseCodec(std::shared_ptr<Compression> compression,
                           std::function<SafeString(size_t)> mRandomGenerator
                                = &SecureRandomGenerator::generate);

    SafeString decode(const db_model::EncryptedBlob& data, const SafeString& key) const;

    db_model::EncryptedBlob encode(const std::string_view& data,
                                   const SafeString& key,
                                   Compression::DictionaryUse dictUse) const;

private:
    static constexpr size_t versionOffset = 0;
    static constexpr size_t ivOffset = versionOffset + versionLength;
    static constexpr size_t tagOffset = ivOffset + EncryptionT::IvLength;
    static constexpr size_t cipherOffset = tagOffset + EncryptionT::AuthenticationTagLength;

    static uint8_t getVersion(const db_model::EncryptedBlob& data);
    static std::string_view getIv(const db_model::EncryptedBlob& data);
    static std::string_view getCipherText(const db_model::EncryptedBlob& data);
    static std::string_view getAuthenticationTag(const db_model::EncryptedBlob& data);

    std::shared_ptr<Compression> mCompression;
    std::function<SafeString(size_t)> const mRandomGenerator;
};


#endif// ERP_PROCESSING_CONTEXT_DATABASECODEC_HXX
