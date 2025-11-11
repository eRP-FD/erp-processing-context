/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/DtbpPseudonymization.hxx"

#include "shared/crypto/AesCbc.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/util/Base64.hxx"

#include <boost/endian.hpp>

std::string DtbpPseudonymization::encryptLogData(std::string_view data, const SafeString& key)
{
    A_27332_01.start("encrypt pseudonymized data");

    std::string cleartext(8, '\0');
    boost::endian::endian_store<std::uint_least64_t, 8, boost::endian::order::big>(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<unsigned char*>(cleartext.data()), data.length());
    cleartext.append(data);
    auto paddingLength = gsl::narrow<size_t>((AesCbc128::BlockSize - (cleartext.length() % AesCbc128::BlockSize)) %
                                             AesCbc128::BlockSize);
    cleartext.append(paddingLength + 16, ' ');

    const std::string iv(AesCbc128::IvLength, '\0');
    auto output = Base64::encode(AesCbc128::encrypt(cleartext, key, iv));
    A_27332_01.finish();

    return output;
}

std::string DtbpPseudonymization::decryptLogData(std::string_view base64Data, const SafeString& key)
{
    const std::string iv(AesCbc128::IvLength, '\0');
    const auto ciphertext = Base64::decodeToString(base64Data);
    auto plainText = AesCbc128::decrypt(ciphertext, key, iv);
    auto plainTextView = static_cast<std::string_view>(plainText);
    auto length = boost::endian::endian_load<std::uint_least64_t, 8, boost::endian::order::big>(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<const unsigned char*>(plainTextView.data()));
    return std::string{plainTextView.substr(8, length)};
}
