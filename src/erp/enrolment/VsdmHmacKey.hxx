/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_VSDMHMACKEY_HXX
#define ERP_PROCESSING_CONTEXT_VSDMHMACKEY_HXX

#include <string>
#include <string_view>
#include <optional>

#include "erp/util/Buffer.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/hsm/HsmSession.hxx"

/**
 * This class is a container class for the export package as defined in
 * A_23466. On creation the availability of operatorId (betreiberkennung) and
 * version is validated. The key and test string are optional to allow
 * for returning such objects without the key in GET operations.
 *
 * The "exp" field is explicitly ignored.
 */
class VsdmHmacKey : public EnrolmentModel
{
public:
    static constexpr auto keyLength = 32;
    struct jsonKey {
        // operator id, expected to be a string with a single character
        static constexpr std::string_view operatorId{"/betreiberkennung"};
        // version of the hmac key, string with a single character
        static constexpr std::string_view version{"/version"};
        // expiry, string, not validated
        static constexpr std::string_view exp{"/exp"};
        // the hmac key encrypted using ecies with the VAU public key, hex encoded string
        static constexpr std::string_view encryptedKey{"/encrypted_key"};
        // decrypted hmac key, only used for storage as blob, base64 encoded string
        static constexpr std::string_view clearTextKey{"/cleartext_key"};
        // result of hmac using the key on an empty string, hex encoded
        static constexpr std::string_view hmacEmptyString{"/hmac_empty_string"};
        // result of hmac using the key on an empty string, but calculated
        // from cleartext key, hex encoded, only used when returning the key package
        static constexpr std::string_view hmacEmptyStringCalculated{"/hmac_empty_string_calculated"};
    };
    explicit VsdmHmacKey(std::string_view jsonText);
    VsdmHmacKey(char operatorId, char version);

    /**
     * @return operatorId
     */
    char operatorId() const;

    /**
     * @return version identifier of the hmac key package
     */
    char version() const;

    /**
     * Delete the hmac encrypted and plaintext keys
     */
    void deleteKeyInformation();

    /**
     * Decrypt key hmac key from the encrypted_key field. The key
     * decryption depends on the ecies key (from the vau certificate).
     * Therefore can only decrypted if "encrypted_key" was encrypted
     * using the current ecies key.
     *
     * @param validate If true, will validate the hmac key against the "hmac_empty_string"
     * value. If the validation fails or hmac_empty_string is not present,
     * an exception is thrown.
     *
     * @return The HMAC VSDM key
     */
    SafeString decryptHmacKey(HsmSession& hsmSession, bool validate) const;

    /**
     * Set the plain text key
     *
     * @param key Base64 encoded string of the plain text key
     */
    void setPlainTextKey(std::string_view key);

    /**
     * Return the plain text key encoded in Base64, if available. Throws
     * an exception of type ErpException if the key is not present.
     *
     * @return std::string_view Base64 encoded plaintext key
     */
    std::string_view plainTextKey() const;

private:
    char mOperatorId = 0;
    char mVersion = 0;
};

#endif
