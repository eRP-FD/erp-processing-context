/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tee/OuterTeeResponse.hxx"

#include "shared/network/message/MimeType.hxx"
#include "shared/crypto/AesGcm.hxx"
#include "shared/crypto/RandomSource.hxx"


OuterTeeResponse::OuterTeeResponse (const std::string& a, const SafeString& aesKey)
{
    const auto iv = RandomSource::defaultSource().randomBytes(AesGcm128::IvLength);
    const auto [ciphertext, authenticationTag] = AesGcm128::encrypt(a, aesKey, iv);
    mEncryptedA.clear();
    mEncryptedA.reserve(iv.size() + ciphertext.size() + authenticationTag.size());
    mEncryptedA.append(iv, iv.size()).append(ciphertext).append(authenticationTag);
}


void OuterTeeResponse::convert (ServerResponse& response) const
{
    response.setHeader(Header::ContentType, MimeType::binary);
    response.setStatus(HttpStatus::OK);
    response.setBody(mEncryptedA);
}
