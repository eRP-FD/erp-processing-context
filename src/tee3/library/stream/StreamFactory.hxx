/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAMFACTORY_HXX
#define EPA_LIBRARY_STREAM_STREAMFACTORY_HXX

#include "library/crypto/tee/TeeContext.hxx"
#include "library/stream/Stream.hxx"

#include <functional>
#include <string>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

class SessionContext;
class TeeSessionContext;

class StreamFactory
{
public:
    /**
     * Return an empty, readable stream.
     */
    static Stream makeReadableStream();
    static Stream makeReadableStream(const char* data, size_t length);
    static Stream makeReadableStream(const unsigned char* data, size_t length);
    static Stream makeReadableStream(const std::string_view& data);
    static Stream makeReadableStream(const char* nullTerminatedData);
    static Stream makeReadableStream(std::string&& data);
    static Stream makeReadableStream(std::vector<Stream>&& streams);
    static Stream makeReadableStream(StreamBuffers buffers);

    static Stream makeWritableStream(
        std::function<void(StreamBuffers&&)>&& writeFunction,
        std::function<void()> notifyEndFunction = {});
    static Stream makeWritableStream(std::streambuf& streambuf);
    static Stream makeWritableStream(std::ostream& stream);

    class Crypto
    {
    public:
        /**
         * Return a stream that encrypts the given `input` with the key that is returned by `symmetricKeySupplier`.
         * The `messageCounter` is used to set up the initialization vector (IV): it will be used as the last 64 bits.
         * The first 32 bits will be set randomly.
         */
        static Stream makeAesGcmEncryptingStream(
            const std::function<EncryptionKey()>& symmetricKeySupplier,
            Stream input,
            uint64_t messageCounter);

        /**
         * Return a stream that will decrypt the content of `input` with the key that is returned by
         * `symmetricKeySupplier`. The optional `messageCounterValidator` will be called with the last 64
         * bit of the IV.
         */
        static Stream makeAesGcmDecryptingStream(
            const std::function<EncryptionKey()>& symmetricKeySupplier,
            Stream input,
            const std::function<void(uint64_t)>& messageCounterValidator = {});

        /**
         * A stream processor that encrypts an input stream of document data.
         */
        static Stream makeDocumentEncryptingStream(const EncryptionKey& key, Stream input);

        /**
        * A stream processor that decrypts an input stream of document data.
        */
        static Stream makeDocumentDecryptingStream(const EncryptionKey& key, Stream input);
    };

    class Base64
    {
    public:
        /**
         * Creates a stream that encodes the given input as Base64 with padding (if necessary).
         * The result of this stream can be used in XML as the "xs:base64Binary" type.
         */
        static Stream makeEncodingStream(Stream input);

        /**
         * Creates a stream that decodes the given Base64 string. Note that the "xs:base64Binary"
         * type allows XML whitespace in the input, so you may want to preprocess the input using
         * String::withoutXmlWhitespace.
         */
        static Stream makeDecodingStream(Stream input);
    };

    class Hash
    {
    public:
        static Stream makeSha1CalculatingStream(
            Stream input,
            std::function<void(const std::string&)> sha1HandlingFunction);
        static Stream makeSha256CalculatingStream(
            Stream input,
            std::function<void(const std::string&)> sha256HandlingFunction);
    };

    class SizeMeasuring
    {
    public:
        static Stream makeSizeMeasuringStream(
            Stream input,
            std::function<void(size_t)> sizeHandlingFunction);
    };

    class Log
    {
    public:
        static Stream makeLoggingStream(const std::string& prefix, Stream input);
        static Stream makeLoggingBinaryStream(const std::string& prefix, Stream input);
    };
};

} // namespace epa

#endif
