/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <string>
#include <string_view>
#include <optional>

#include "shared/enrolment/EnrolmentModel.hxx"
#include "shared/hsm/HsmSession.hxx"

/**
 * This class is a container class for the Pseudoname Log key export package
 * as defined in A_27392-01. On creation, version needs to be present.
 * The key and test string are optional to allow
 * for returning such objects without the key in GET operations.
 */
class PseudonameLogKeyPackage : public EnrolmentModel
{
public:
    static constexpr auto keyLength = Aes128Length;
    struct JsonKey {
        // version of the key, numeric string
        static constexpr std::string_view version{"/key_version"};
        // the hmac key encrypted using ecies with the VAU public key, hex encoded string
        static constexpr std::string_view encryptedKey{"/encrypted_key"};
        // result of hmac using the key on an empty string, hex encoded
        static constexpr std::string_view encCertHash{"/enc_cert_hash"};
    };
    explicit PseudonameLogKeyPackage(std::string_view jsonText);

    /**
     * @return version identifier of the pseudoname log key package
     */
    unsigned int version() const;

    /**
     * Decrypt Pseudoname log key from the encrypted_key field. The key
     * decryption depends on the ecies key (from the vau certificate).
     * Therefore can only decrypted if "encrypted_key" was encrypted
     * using the current ecies key.
     *
     * @return The Pseudoname Log key
     */
    SafeString decryptPseudonameLogKey(HsmSession& hsmSession) const;

private:
    unsigned int mVersion = 0;
};
