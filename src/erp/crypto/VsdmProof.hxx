/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSIONG_CONTEXT_CRYPTO_VSDM_PROOF_HXX
#define ERP_PROCESSIONG_CONTEXT_CRYPTO_VSDM_PROOF_HXX

#include <chrono>
#include <string>
#include <string_view>

#include "shared/util/SafeString.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/model/Kvnr.hxx"

class VsdmHmacKey;

struct VsdmProof2
{
private:
    static constexpr std::chrono::seconds iatEpochOffset{1735689600};
    static constexpr std::string_view derivationKey{"VSDM+ Version 2 AES/GCM"};
    static constexpr auto encryptedContentLength = 18;
    static constexpr auto hcvSize = 5;
    static constexpr auto binarySize = 47;
public:
    [[nodiscard]] char keyOperatorId() const;
    [[nodiscard]] char keyVersion() const;

    struct DecryptedProof{
        bool revoked;
        std::string hcv;
        model::Timestamp iat;
        model::Kvnr kvnr;
    };

    /**
     * Takes a binary array and returns a VsdmProof2 object.
     * No decryption is taking place.
     */
    static VsdmProof2 deserialize(std::string_view validationData);

    /**
     * With the given shared secret, unpack the encrypted data
     * from the serialized information.
     * Throws if an error occurs.
     */
    [[nodiscard]] DecryptedProof decrypt(const SafeString& sharedSecret) const;

    /**
     * With the given vsdm Key, derive the encryption key and
     * generate a VSDM proof.
     */
    static VsdmProof2 encrypt(const VsdmHmacKey& vsdmKey, const DecryptedProof& proofData);

    /**
     * From the encrypted data and key information, generate the base64-encoded
     * proof data.
     */
    std::string serialize();

    static std::string makeHcv(std::string_view insuranceStart, std::string_view streetAddress);

private:
    char mKeyOperatorId{};
    char mKeyVersion{};

    std::string mIv;
    std::string mCiphertext;
    std::string mAuthenticationTag;
};


#endif