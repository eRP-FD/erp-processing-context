/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_AESGCMSTREAMCRYPTOR_HXX
#define EPA_LIBRARY_CRYPTO_AESGCMSTREAMCRYPTOR_HXX

#include "library/crypto/CryptoTypes.hxx"
#include "library/crypto/SensitiveDataGuard.hxx"
#include "library/stream/Stream.hxx"
#include "library/stream/StreamBuffers.hxx"
#include "library/util/StrongType.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <cstddef>
#include <functional>

namespace epa
{

/**
 * AES GCM 256bit encryption and decryption for byte streams.

 * There are convenience methods `encrypt` and `decrypt` for simple strings.
 * But these are intended for tests.
 * Alternatively you can use the AesGcmCryptor for simple strings.
 */
class AesGcmStreamCryptor
{
public:
    using Key = EncryptionKey;

    static constexpr std::size_t NonceLength = 96 / 8; // 96 bits = 12 bytes
    static constexpr std::size_t HashLength = 128 / 8; // 128 bits = 16 bytes

    /**
     * The initialization vector, also known as IV or as nonce,
     * is expected to be 96 bits = 12 bytes long.
     */
    using InitializationVector = std::string;

    /**
     * Create an initialization vector that is a concatenation of
     * - 32 bit of random data
     * - the given 64 bit `messageCounter` (big-endian)
     *
     * This conforms to A_24628.
     */
    static InitializationVector createSemiRandomIV(const uint64_t messageCounter);

    /**
     * Create an initialization vector that consists of 96 bit of random data.
     */
    static InitializationVector createRandomIV();
    static std::string createRandomKey();

    /**
      * Return a stream that wraps the given `input` stream and encrypts it on the fly.
      * Supports both readable and writable input streams.
      *
      * @param ivSupplier that uses a combination of message counter and 32 random bits.
      *        Only supplied to allow unit tests to provide a fixed IV.
      *        Don't provide a custom supplier in production.
      * @param aadSupplier If present, provides buffers that are treated as additional
      *        authentication data. The data is used before the first byte is encrypted.
      */
    static Stream makeEncryptingStream(
        const std::function<Key()>& symmetricKeySupplier,
        Stream input,
        uint64_t messageCounter,
        const std::function<InitializationVector(uint64_t)>& ivSupplier = createSemiRandomIV,
        const std::function<StreamBuffers()>& aadSupplier = {});

    /**
      * Return a stream that wraps the given `input` stream and encrypts it on the fly.
      * Supports both readable and writable input streams.
      *
      * @param ivSupplier that uses full 96 bits of random bits.
      *        Only supplied to allow unit tests to provide a fixed IV.
      *        Don't provide a custom supplier in production.
     * @param aadSupplier If present, provides buffers that are treated as additional
     *        authentication data. The data is used before the first byte is encrypted.
      */
    static Stream makeEncryptingStream(
        const std::function<Key()>& symmetricKeySupplier,
        Stream input,
        const std::function<InitializationVector()>& ivSupplier = createRandomIV,
        const std::function<StreamBuffers()>& aadSupplier = {});

    /**
     * Return a stream that wraps the given `input` stream and decrypts it on the fly.
     * Supports both readable and writable input streams.
     * When the optional `messageCounterValidator` is present, then it will be called with the
     * last 64 bit of the IV.
     *
     * @param messageCounterValidator this is a version <3 feature. It will be removed for
     *        version 3 and until then must not be used.
     * @param aadSupplier  If present, provides buffers that are treated as additional
     *        authentication data. The data is used before the first byte is encrypted.
     */
    static Stream makeDecryptingStream(
        const std::function<Key()>& symmetricKeySupplier,
        Stream input,
        const std::function<void(uint64_t)>& messageCounterValidator = {},
        const std::function<StreamBuffers()>& aadSupplier = {},
        const std::function<void(std::exception_ptr)>& authenticationTagMismatchHandler = {});

    using unique_cipher_context = std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)>;


    class Encryptor
    {
    public:
        explicit Encryptor(
            std::function<Key()> symmetricKeySupplier,
            uint64_t messageCounter,
            std::function<InitializationVector(uint64_t)> ivSupplier = createSemiRandomIV,
            std::function<StreamBuffers()> aadSupplier = {});

        explicit Encryptor(
            std::function<Key()> symmetricKeySupplier,
            std::function<InitializationVector()> ivSupplier = createRandomIV,
            std::function<StreamBuffers()> aadSupplier = {});

        /**
         * This copy constructor does not create a carbon copy of the `other` object.
         * It only copies over the data the it can't create on its own, i.e. the key supplier.
         * The internal state is initialized so that the copy is ready to start encryption.
         *
         * Implementation note: This constructor only exists to interface with the
         * ProcessingStreamImplementation and its use with std::function
         * which does not work with move-only types.
         */
        Encryptor(const Encryptor& other);

        ~Encryptor() = default;

        Encryptor& operator=(const Encryptor& other);

        StreamBuffers operator()(StreamBuffers buffers);

    private:
        std::function<Key()> mKeySupplier;
        std::function<InitializationVector()> mIvSupplier;
        std::function<StreamBuffers()> mAadSupplier;
        enum class State
        {
            NotStarted,
            Initialized,
            Finished
        };
        State mState;
        unique_cipher_context mCipherContext;
        const std::size_t mBlockSize;
        size_t mAccumulatingMessageSize;

        StreamBuffer start();
        StreamBuffer encryptBuffers(StreamBuffers& buffers);
        void addAdditionalAuthenticationData(StreamBuffers buffers);
        StreamBuffer finalize();
        StreamBuffer getHashBuffer();
    };


    class Decryptor
    {
    public:
        /**
         * Create a Decryptor instance.
         * @param symmetricKeySupplier provides the symmetric key that is used to decrypt the stream.
         * @param messageCounterValidator an optional lambda that is called to validate the message
         *     counter that is contained in the last 64 bits in the IV. Used by the old TEE 1/2
         *     protocol.
         * @param aadSupplier an optional supplier of additional authentication data. If present,
         *     the data is added before any data of the stream is added.
         * @param authenticationTagMismatchHandler an optional exception handler that can
         *     translate exceptions that are throw while the authentication tag is processed.
         */
        explicit Decryptor(
            std::function<Key()> symmetricKeySupplier,
            std::function<void(uint64_t)> messageCounterValidator,
            std::function<StreamBuffers()> aadSupplier = {},
            std::function<void(std::exception_ptr)> authenticationTagMismatchHandler = {});

        /**
         * This copy constructor does not create a carbon copy of the `other` object.
         * It only copies over the data the it can't create on its own, i.e. the key supplier.
         * The internal state is initialized so that the copy is ready to start encryption.
         *
         * Implementation note: This constructor only exists to interface with the
         * ProcessingStreamImplementation and its use with std::function
         * which does not work with move-only types.
         */
        Decryptor(const Decryptor& other);

        ~Decryptor() = default;

        StreamBuffers operator()(StreamBuffers buffers);

    private:
        std::function<Key()> mKeySupplier;
        std::function<StreamBuffers()> mAadSupplier;
        std::function<void(std::exception_ptr)> mAuthenticationTagMismatchHandler;
        enum class State
        {
            NotStarted,
            Initialized,
            Finished
        };
        State mState;
        unique_cipher_context mCipherContext;
        StreamBuffers mPendingBuffers;
        const std::size_t mBlockSize;
        std::function<void(uint64_t)> mMessageCounterValidator;
        size_t mAccumulatingMessageSize;

        void start(const StreamBuffers& ivBuffers);
        StreamBuffer decryptBuffers(const StreamBuffers& messageBuffers);
        void addAdditionalAuthenticationData(StreamBuffers buffers);
        StreamBuffer finalizeAndVerify(std::string& hash);

        /**
         * The last 16 bytes in a stream contain the authentication tag, which has to be
         * treated differently from the ciphertext that precedes the tag. As we don't know
         * how long the stream is until we have seen its end, we have to keep the last 16 bytes that
         * we have seen in "reserve".
         */
        static std::tuple<StreamBuffers, StreamBuffers> splitOffHash(StreamBuffers buffers);
    };

    /**
     * All AES GCM related errors are throw as AesGcmStreamCryptor::Exception
     * to give code higher up the call chain
     * to convert these into the appropriate gematik errors.
     */
    class Exception : public std::runtime_error
    {
    public:
        explicit Exception(const std::string& what);
    };

    /** Encrypts the given data. Returns a concatenation of IV, ciphertext, and auth tag. */
    static std::string encrypt(const std::string& plainText, const Key& key);
    static std::string decrypt(const std::string& encryptedText, const Key& key);

    static std::size_t adjustSizeForEncryption(std::size_t sizeBeforeEncryption);
    static std::size_t adjustSizeForDecryption(std::size_t sizeBeforeDecryption);
};

} // namespace epa

#endif
