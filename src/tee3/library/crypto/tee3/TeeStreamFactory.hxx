/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEESTREAMFACTORY_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEESTREAMFACTORY_HXX

#include "library/crypto/tee3/Tee3SessionContext.hxx"
#include "library/stream/Stream.hxx"

#include <functional>

#include "shared/network/message/Header.hxx"

namespace epa
{

class BinaryBuffer;
struct Tee3Context;


/**
 * A factory of streams that encrypt or decrypt data with the K2 keys.
 * There are four different streams, for encryption and decryption and for reading and writing.
 */
class TeeStreamFactory
{
public:
    /**
     * The header is described in A_24628 (gemSpec_Krypt).
     * Define two frequently used sizes.
     */
    // clang-format off
    constexpr static size_t headerSize
        = 3*1   // version, isPU, direction
          + 8   // message counter
          + 32; // KeyID

    // GEMREQ-start A_24619#encryptionOverhead
    constexpr static size_t encryptionOverhead
        = headerSize
          + 12  // IV
          + 16; // AES-GCM tag
    // GEMREQ-end A_24619#encryptionOverhead
    // clang-format on

    /**
     * Wrap a header and a body stream into a combined stream. This is about serialization of the
     * header and concatenation with the body stream. Encryption does not take place.
     */
    static std::tuple<size_t, Stream> wrapRequestHeaderAndBody(
        const Header& header,
        size_t bodyLength,
        Stream&& body);

    /**
     * For the decrypting streams we need a way to look up a TEE context for the KeyID in the
     * message header.
     */
    using TeeContextProviderForKeyId = std::function<Tee3SessionContext(const KeyId& keyId)>;
    /**
     * For encrypting streams we also need a TEE context. The KeyId argument is not required.
     */
    using TeeContextProvider = std::function<Tee3SessionContext()>;

    /**
     * Return a readable stream that encrypts the content of the `delegate` stream. It can be used
     * for either `direction`: client to server (Request) and server to client (Response).
     */
    static Stream createReadableEncryptingStream(
        TeeContextProvider teeContextProvider,
        Stream&& delegate);

    /**
     * Return a writable stream that encrypts the content that is written to it and writes the
     * ciphertext to the `delegate` stream. It can be used for either `direction`: client to
     * server (Request) and server to client (Response).
     */
    static Stream createWritableEncryptingStream(
        TeeContextProvider teeContextProvider,
        Stream&& delegate);

    /**
     * Return a readable stream that decrypts the content of the `delegate` stream. It can be used
     * for either `direction`: client to server (Request) and server to client (Response).
     */
    static Stream createReadableDecryptingStream(
        TeeContextProviderForKeyId contextProvider,
        Stream&& delegate);

    /**
     * Return a writable stream that decrypts the content that is written to it and writes the
     * plaintext to the `delegate` stream. It can be used for either `direction`: client to
     * server (Request) and server to client (Response).
     */
    static Stream createWritableDecryptingStream(
        TeeContextProviderForKeyId contextProvider,
        Stream&& delegate);

    enum RequestOrResponse
    {
        Request,
        Response
    };

    /**
     * The returned stream splits the data in its `delegate` stream into HTTP header and body stream.
     * The header is given to the optional `headerConsumer`. The body is forwarded to the `delegate`
     * stream. The returned stream is readable.
     */
    static Stream createReadableHeaderBodySplittingStream(
        Stream&& delegate,
        std::optional<uint64_t> bodyLimit,
        std::function<void(Header&&)>&& headerConsumer,
        RequestOrResponse requestOrResponse);

    /**
     * The returned stream splits the data in its `delegate` stream into HTTP header and body stream.
     * The header is given to the optional `headerConsumer`. The body is forwarded to the `delegate`
     * stream. The returned stream is writable.
     */
    static Stream createWritableHeaderBodySplittingStream(
        Stream&& delegate,
        uint64_t bodyLimit,
        std::function<void(Header&&)>&& headerConsumer,
        RequestOrResponse requestOrResponse);
};

} // namespace epa
#endif
