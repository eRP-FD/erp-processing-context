/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/DatabaseCodec.hxx"

#include "erp/database/DatabaseModel.hxx"
#include "erp/util/Expect.hxx"

DataBaseCodec::DataBaseCodec(std::shared_ptr<Compression> compression,
                             std::function<SafeString(size_t)> randomGenerator)
    : mCompression(std::move(compression))
    , mRandomGenerator(std::move(randomGenerator))
{
}

uint8_t DataBaseCodec::getVersion(const db_model::EncryptedBlob& data)
{
    return uint8_t(data[versionOffset]);
}

std::string_view DataBaseCodec::getIv(const db_model::EncryptedBlob& data)
{
    return std::string_view{reinterpret_cast<const char*>(data.data()) + ivOffset, EncryptionT::IvLength};
}

std::string_view DataBaseCodec::getAuthenticationTag(const db_model::EncryptedBlob& data)
{
    return std::string_view{reinterpret_cast<const char*>(data.data()) + tagOffset, EncryptionT::AuthenticationTagLength};
}

std::string_view DataBaseCodec::getCipherText(const db_model::EncryptedBlob& data)
{
    return std::string_view{reinterpret_cast<const char*>(data.data()) + cipherOffset, data.size() - cipherOffset};
}

SafeString DataBaseCodec::decode(const db_model::EncryptedBlob& data, const SafeString& key) const
{
    Expect3(data.size() >= minEncodedLength, "not enough data for decoding.", std::logic_error);
    auto dataVersion = getVersion(data);
    Expect3(dataVersion == version, "database blob version unknown: " + std::to_string(dataVersion), std::logic_error);
    SafeString decrypted{EncryptionT::decrypt(getCipherText(data), key, getIv(data), getAuthenticationTag(data))};
    return SafeString{mCompression->decompress(decrypted)};
}

db_model::EncryptedBlob DataBaseCodec::encode(const std::string_view& data,
                                              const SafeString& key,
                                              Compression::DictionaryUse dictUse) const
{
    SafeString compressed{mCompression->compress(data, dictUse)};
    auto iv{mRandomGenerator(EncryptionT::IvLength)};
    auto cipherResult = EncryptionT::encrypt(compressed, key, iv);
    db_model::EncryptedBlob blob;
    blob.reserve(versionLength + iv.size() + cipherResult.authenticationTag.size() + cipherResult.ciphertext.size());
    blob.push_back(char(version));
    blob.insert(blob.end(), static_cast<const char*>(iv), static_cast<const char*>(iv) + iv.size());
    blob.insert(blob.end(), cipherResult.authenticationTag.begin(), cipherResult.authenticationTag.end());
    blob.insert(blob.end(), cipherResult.ciphertext.begin(), cipherResult.ciphertext.end());
    return blob;
}
