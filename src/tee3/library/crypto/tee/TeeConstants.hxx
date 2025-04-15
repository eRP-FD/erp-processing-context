/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE_TEECONSTANTS_HXX
#define EPA_LIBRARY_CRYPTO_TEE_TEECONSTANTS_HXX

// Users of this header are likely to also require constants from AesGcmStreamCryptor.
#include "library/crypto/AesGcmStreamCryptor.hxx"

#include <cstddef>
#include <string_view>

namespace epa
{

/// The name of this namespace is vau instead of tee because there is already a function called tee.
/// Unless someone comes up with a better name, it will be the German version of tee: vau.

// GEMREQ-start A_17070-02, A_17074
namespace vau
{
    namespace key
    {
        constexpr std::string_view dataType = "DataType";
        constexpr std::string_view cipherConfiguration = "CipherConfiguration";
        constexpr std::string_view publicKey = "PublicKey";
        constexpr std::string_view messageType = "MessageType";
        constexpr std::string_view data = "Data";
        constexpr std::string_view authorizationAssertion = "AuthorizationAssertion";
        constexpr std::string_view certificateHash = "CertificateHash";
        constexpr std::string_view vauClientHelloDataHash = "VAUClientHelloDataHash";
        constexpr std::string_view vauServerHelloDataHash = "VAUServerHelloDataHash";
        constexpr std::string_view signature = "Signature";
        constexpr std::string_view certificate = "Certificate";
        constexpr std::string_view ocspResponse = "OCSPResponse";
        constexpr std::string_view finishedData = "FinishedData";
        constexpr std::string_view time = "Time";
    }
    // GEMREQ-end A_17070-02, A_17074

    namespace value
    {
        constexpr std::string_view vauClientHello = "VAUClientHello";
        constexpr std::string_view vauClientHelloData = "VAUClientHelloData";
        constexpr std::string_view vauServerHello = "VAUServerHello";
        constexpr std::string_view vauServerHelloData = "VAUServerHelloData";
        constexpr std::string_view vauClientSigFin = "VAUClientSigFin";
        constexpr std::string_view vauServerFin = "VAUServerFin";
        constexpr std::string_view vauServerError = "VAUServerError";
        constexpr std::string_view vauServerErrorData = "VAUServerErrorData";
        // GEMREQ-start A_16901, A_15547
        constexpr std::string_view brainpoolSha256 = "AES-256-GCM-BrainpoolP256r1-SHA-256";
        // GEMREQ-end A_16901, A_15547

        // GEMREQ-start A_16943-01#keyDerivation, A_16943-01
        constexpr const char* keyDerivationKeyId = "KeyID";
        constexpr const char* keyDerivationClientToServer = "AES-256-GCM-Key-Client-to-Server";
        constexpr const char* keyDerivationServerToClient = "AES-256-GCM-Key-Server-to-Client";
        // GEMREQ-end A_16943-01#keyDerivation, A_16943-01
    }

    namespace error
    {
        constexpr const char* aesGcmDecryptionError = "AES-GCM decryption error";
        // GEMREQ-start A_17072_01
        constexpr const char* clientCertificateInconsistent = "Client Certificate inconsistent";
        constexpr const char* clientSigFinInvalid = "VAUClientSigFin invalid";
        constexpr const char* clientSignatureInvalid = "Signature from VAUClientSigFin invalid";
        // GEMREQ-end A_17072_01
        constexpr const char* httpAdditionalHeaderLengthError =
            "HTTP additional header length error";
        constexpr const char* invalidAuthenticationSubject =
            "invalid subject name id in authentication assertion compared to authorization "
            "assertion";
        constexpr const char* invalidCertificateIdentity =
            "invalid certificate identity compared to authorization identity";
        constexpr const char* invalidCounterValue = "invalid counter value";
        constexpr const char* invalidProtocollVersion =
            "invalid protocoll version"; /// The superfluous 'l' is taken verbatim from the spec.
        constexpr const char* messageCounterOverflow = "message counter overflow";
        constexpr const char* keyIdNotFoundTemplate = "KeyID XXX not found";
        constexpr const char* unknownTeeContext = "unknown tee context is used";
    }

    constexpr size_t keyIdLength =
        256 / 8; // The KeyId at the head of a TEE encrypted body is 256 bit = 32 byte long.
    constexpr size_t AesGcmKeyLength = 32;
    constexpr size_t AesGcmNonceLength = 96 / 8; // Nonce used by the AES GCM encryption algorithm.
    constexpr size_t AesGcmHashLength =
        128 / 8; // HashNonce used by the AES GCM encryption algorithm.

    constexpr uint8_t protocolVersion =
        0x01; // There is currently only one version (0x01). See e.g. A_16952
    constexpr size_t protocolVersionLength = sizeof(uint8_t);
    constexpr size_t counterLength = sizeof(uint64_t);
    constexpr size_t headerLengthLength = sizeof(uint32_t);
}

} // namespace epa

#endif
