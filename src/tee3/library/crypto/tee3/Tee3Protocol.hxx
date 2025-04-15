/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEE3PROTOCOL_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEE3PROTOCOL_HXX

#include "library/crypto/SensitiveDataGuard.hxx"
#include "library/crypto/tee3/TeeError.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <boost/regex.hpp>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

/**
 * A collection of simple data types that are used during the TEE 3 handshake.
 *
 * The choice of data types and their names is influenced by the Gematik reference implementation.
 */
class Tee3Protocol
{
public:
    static constexpr std::string_view message1Name = "M1";
    // GEMREQ-start A_24608
    static constexpr std::string_view message2Name = "M2";
    // GEMREQ-end A_24608
    static constexpr std::string_view message3Name = "M3";
    // GEMREQ-start A_24626
    static constexpr std::string_view message4Name = "M4";
    // GEMREQ-end A_24626
    static constexpr std::string_view messageRestartName = "Restart";

    struct EcdhPublicKey
    {
        std::string_view curve = "P-256";
        BinaryBuffer x;
        BinaryBuffer y;


        template<typename Processor>
        constexpr void processMembers(Processor& processor)
        {
            processor("/crv", curve);
            processor("/x", x);
            processor("/y", y);
        }

        static EcdhPublicKey fromEvpPkey(const shared_EVP_PKEY& key);
        shared_EVP_PKEY toEvpPkey() const;
        BinaryBuffer serialize() const;

        bool operator==(const EcdhPublicKey& other) const = default;
    };


    /**
     * A_24425
     */
    struct VauKeys
    {
        // The answer to ANFEPA-2362 makes it clear that ecdhPublicKey is to be serialized as
        // nested map, not as a string that contains the serialized public key.
        EcdhPublicKey ecdhPublicKey;
        BinaryBuffer kyber768PublicKey;
        int64_t iat = 0;
        int64_t exp = 0;
        std::string comment;


        template<typename Processor>
        constexpr void processMembers(Processor& processor)
        {
            processor("/ECDH_PK", ecdhPublicKey);
            processor("/Kyber768_PK", kyber768PublicKey);
            processor("/iat", iat);
            processor("/exp", exp);
            processor("/comment", comment);
        }
    };


    struct SignedPublicKeys
    {
        BinaryBuffer signedPublicKeys;
        BinaryBuffer signatureES256;
        BinaryBuffer certificateHash;
        uint64_t cdv = 0;
        BinaryBuffer ocspResponse;


        template<typename Processor>
        constexpr void processMembers(Processor& processor)
        {
            processor("/signed_pub_keys", signedPublicKeys);
            processor("/signature-ES256", signatureES256);
            processor("/cert_hash", certificateHash);
            processor("/cdv", cdv);
            processor("/ocsp_response", ocspResponse);
        }
    };


    struct PrivateKeys
    {
        SensitiveDataGuard ecdhPrivateKey;
        SensitiveDataGuard kyber768PrivateKey;


        template<typename Processor>
        constexpr void processMembers(Processor& processor)
        {
            processor("/ECDH_PrivKey", ecdhPrivateKey);
            processor("/Kyber768_PrivKey", kyber768PrivateKey);
            processor("/ECDH_PK");
            processor("/Kyber768_PK");
        }
    };

    struct Keypairs
    {
        struct
        {
            EcdhPublicKey publicKey;
            SensitiveDataGuard privateKey;
        } ecdh;

        struct
        {
            BinaryBuffer publicKey;
            SensitiveDataGuard privateKey;
        } kyber768;


        static Keypairs generate();
        PrivateKeys privateKeys() const;
    };

    struct SharedKeys
    {
        SensitiveDataGuard ecdhSharedSecret;
        SensitiveDataGuard kyber768SharedSecret;
    };
    /**
     * The ECDH cipher text is not CBOR encoded. It contains the ECDH public key directly
     * as embedded structure. This has been clarified per eMail on 9.7.2024 by Andreas Hallof.
     */
    struct CipherTexts
    {
        EcdhPublicKey ecdhCipherText;
        BinaryBuffer kyber768CipherText;
    };
    struct Encapsulation
    {
        SharedKeys sharedKeys;
        CipherTexts cipherTexts;
    };

    /**
     * This structure holds the data that is exchanged via the /CertData.<hash>-<version> endpoint
     * for A_24957.
     *
     * Also known as AUT-VAU-Zertifikat.
     *
     * The `certificate`, `ca` and `rcaChain` fields are used as follows:
     * - `certificate` is the focus of of this data structure. It is the certificate that contains
     *   the public key of the private signer key.
     * - `ca` is an intermediate certificate that is the link between the RCA certificates and
     *   `certificate`.
     * - `rcaChain` is a set of cross-signed RCA certificates that are based on a self signed
     *   trust anchor (not included) that the verifiying side (the client) is expected to have.
     *   On the other side (the last certificate in the chain) we have a signing certificate of
     *   `ca`.
     *
     * For production `rcaChain` is expected to contain Gematik's RCA<N> certificates. Its first
     * is RCA6, cross signed by RCA5 (not included).
     * For testing `rcaChain` contains Achelos RCA certificates.
     */
    struct AutTeeCertificate
    {
        BinaryBuffer certificate;
        BinaryBuffer ca;
        std::vector<BinaryBuffer> rcaChain;


        template<typename Processor>
        constexpr void processMembers(Processor& processor)
        {
            processor("/cert", certificate);
            processor("/ca", ca);
            processor("/rca_chain", rcaChain);
        }
    };


    class VauCid
    {
    public:
        VauCid();
        explicit VauCid(const std::string& value);
        VauCid(const VauCid& other) = default;
        explicit VauCid(std::string&& value);
        VauCid(VauCid&& other) noexcept = default;
        ~VauCid() = default;

        VauCid& operator=(const VauCid& other);
        VauCid& operator=(VauCid&& other) noexcept;

        bool operator==(const VauCid& other) const = default;

        bool empty() const;
        const std::string& toString() const;

        /**
         * Verify conditions from requirements
         * A_24608,A_24622: The returned value must be a valid URL and not be longer than 200
         * characters. It may only contain characters a-zA-Z0-9-/. The first character has to be a
         * slash.
         */
        void verify() const;

    private:
        std::string mValue;
        const static boost::regex mVauCidRegex;
    };


    /**
     * Successor to library/crypto/tee/SymmetricKeys, but with more fitting data types for the keys.
     */
    // GEMREQ-start A_15549-01#secureDeletion
    struct SymmetricKeys
    {
        SensitiveDataGuard clientToServer;
        SensitiveDataGuard serverToClient;

        bool operator==(const SymmetricKeys& other) const = default;
    };
    // GEMREQ-end A_15549-01#secureDeletion

    class Exception : public std::runtime_error
    {
    public:
        explicit Exception(const std::string& message, const HttpStatus status = HttpStatus::OK);

        HttpStatus status;
    };

    /**
     * Use this exception to signal that an abortion of the
     * TEE protocol for the current context is required according to A_16849.
     */
    class AbortingException : public Tee3Protocol::Exception
    {
    public:
        explicit AbortingException(const std::string& message);
    };
};

} // namespace epa

#endif
