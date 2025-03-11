/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/TeeStreamFactory.hxx"
#include "library/crypto/AesGcmStreamCryptor.hxx"
#include "library/crypto/tee3/HeaderBodySplittingStream.hxx"
#include "library/crypto/tee3/Tee3SessionContext.hxx"
#include "library/stream/StreamFactory.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/Serialization.hxx"

#include "shared/network/message/Header.hxx"

namespace epa
{

namespace
{
    /**
     * StreamImplementation that encrypts a delegate stream with one of the K2 TEE keys.
     * The choice of key (c2s or s2c) depends on the direction of the stream.
     * The implementation supports readable and writable streams, but not at the same time.
     */
    class TeeEncryptingStreamImplementation : public StreamImplementation
    {
    public:
        TeeEncryptingStreamImplementation(
            TeeStreamFactory::TeeContextProvider teeContextProvider,
            Stream&& delegate,
            ReadableOrWritable mode);

        bool isReadSupported() const override;
        bool isWriteSupported() const override;

    protected:
        StreamBuffers doRead() override;
        void doWrite(StreamBuffers&& plaintextBuffers) override;

    private:
        TeeStreamFactory::TeeContextProvider mTeeContextProvider;
        Stream mDelegate;
        bool mIsProcessingBody;
        const ReadableOrWritable mMode;

        static StreamBuffers createHeader(const Tee3SessionContext& context);
        void activateEncryption(
            StreamBuffers&& additionalAuthenticationData,
            const Tee3SessionContext& context);
    };


    class TeeDecryptingStreamImplementation : public ReadWritableStreamImplementation
    {
    public:
        TeeDecryptingStreamImplementation(
            TeeStreamFactory::TeeContextProviderForKeyId teeContextProvider,
            Stream&& delegate,
            ReadableOrWritable mode);

        bool isReadSupported() const override;
        bool isWriteSupported() const override;

    protected:
        StreamBuffers doRead() override;
        void doWrite(StreamBuffers&& buffers) override;

    private:
        TeeStreamFactory::TeeContextProviderForKeyId mTeeContextProvider;
        Stream mDelegate;
        bool mIsProcessingBody;
        /// On writes() we may not get enough bytes for the Tee header. Accumulate the first
        /// writen buffers until we have the complete header.
        StreamBuffers mHeaderBuffer;
        const ReadableOrWritable mMode;
        /// The request counter will be initialized from the outer TEE header.
        uint64_t mRequestCounter;

        void processTeeHeader(StreamBuffers&& buffers);

        /**
         * Decryption is only activated after the header has been processed.
         * The header itself is transmitted as plaintext.
         */
        void activateDecryption(
            const Tee3SessionContext& context,
            StreamBuffers&& additionalAuthenticationData);
    };

    // GEMREQ-start A_24630#assertDirection
    void assertDirection(
        const uint8_t givenDirection,
        const Tee3SessionContext::Direction expectedDirection)
    {
        if (givenDirection != static_cast<uint8_t>(expectedDirection))
        {
            const static std::string message = "invalid TEE direction";
            LOG(ERROR) << message;
            if (expectedDirection == Tee3SessionContext::ClientToServer)
                throw TeeError(TeeError::Code::NotARequest, message);
            else
            {
                // This is an error that can only occur on the client side (at least as long as
                // a TEE instance can not call another TEE instance via a TEE channel). Therefore,
                // there is no TeeError::Code for it.
                throw std::runtime_error(message);
            }
        }
    }
    // GEMREQ-end A_24630#assertDirection
}


// GEMREQ-start A_24628-01#wrapRequestHeaderAndBody
std::tuple<size_t, Stream> TeeStreamFactory::wrapRequestHeaderAndBody(
    const Header& header,
    size_t bodyLength,
    Stream&& body)
{
    // Serialize the header.
    // Should we use boost::beast::serializer for this?
    std::stringstream s;
    s << toString(header.method()) << " " << header.target() << " HTTP/";
    if (header.version() == 10)
        s << "1.0";
    else if (header.version() == 11)
        s << "1.1";
    else
        Failure() << "unsupported HTTP version";
    s << "\r\n";
    s << header.serializeFields();
    s << "\r\n";
    const size_t headerLength = s.str().size();

    // Concatenate with the body stream.
    std::vector<Stream> streams;
    streams.emplace_back(StreamFactory::makeReadableStream(s.str()));
    streams.emplace_back(std::move(body));
    auto concatenatingStream = StreamFactory::makeReadableStream(std::move(streams));
    return {headerLength + bodyLength, std::move(concatenatingStream)};
}
// GEMREQ-end A_24628-01#wrapRequestHeaderAndBody


Stream TeeStreamFactory::createReadableEncryptingStream(
    TeeStreamFactory::TeeContextProvider teeContextProvider,
    Stream&& delegate)
{
    return Stream(std::make_unique<TeeEncryptingStreamImplementation>(
        std::move(teeContextProvider), std::move(delegate), ReadableOrWritable::Readable));
}


Stream TeeStreamFactory::createWritableEncryptingStream(
    TeeStreamFactory::TeeContextProvider teeContextProvider,
    Stream&& delegate)
{
    return Stream(std::make_unique<TeeEncryptingStreamImplementation>(
        std::move(teeContextProvider), std::move(delegate), ReadableOrWritable::Writable));
}


Stream TeeStreamFactory::createReadableDecryptingStream(
    TeeContextProviderForKeyId contextProvider,
    Stream&& delegate)
{
    return Stream(std::make_unique<TeeDecryptingStreamImplementation>(
        std::move(contextProvider), std::move(delegate), ReadableOrWritable::Readable));
}


Stream TeeStreamFactory::createWritableDecryptingStream(
    TeeContextProviderForKeyId contextProvider,
    Stream&& delegate)
{
    return Stream(std::make_unique<TeeDecryptingStreamImplementation>(
        std::move(contextProvider), std::move(delegate), ReadableOrWritable::Writable));
}


Stream TeeStreamFactory::createReadableHeaderBodySplittingStream(
    Stream&& delegate,
    const std::optional<uint64_t> bodyLimit,
    std::function<void(Header&&)>&& headerConsumer,
    const RequestOrResponse requestOrResponse)
{
    return Stream(std::make_unique<HeaderBodySplittingStreamImplementation>(
        std::move(delegate),
        bodyLimit,
        std::move(headerConsumer),
        ReadableOrWritable::Readable,
        requestOrResponse == Request,
        true));
}


Stream TeeStreamFactory::createWritableHeaderBodySplittingStream(
    Stream&& delegate,
    const uint64_t bodyLimit,
    std::function<void(Header&&)>&& headerConsumer,
    const RequestOrResponse requestOrResponse)
{
    return Stream(std::make_unique<HeaderBodySplittingStreamImplementation>(
        std::move(delegate),
        bodyLimit,
        std::move(headerConsumer),
        ReadableOrWritable::Writable,
        requestOrResponse == Request));
}


namespace
{
    TeeEncryptingStreamImplementation::TeeEncryptingStreamImplementation(
        TeeStreamFactory::TeeContextProvider teeContextProvider,
        Stream&& delegate,
        const ReadableOrWritable mode)
      : mTeeContextProvider(std::move(teeContextProvider)),
        mDelegate(std::move(delegate)),
        mIsProcessingBody(false),
        mMode(mode)
    {
        // Note that mDelegate at this point is the original delegate stream. That is because
        // we have to write the outer header without encryption. When the header is written,
        // the method activateEncryption() will be called to activate the encryption of the
        // original delegate stream.
    }

    bool TeeEncryptingStreamImplementation::isReadSupported() const
    {
        return mMode == Readable;
    }

    bool TeeEncryptingStreamImplementation::isWriteSupported() const
    {
        return mMode == Writable;
    }

    // GEMREQ-start A_24628-01#doRead
    StreamBuffers TeeEncryptingStreamImplementation::doRead()
    {
        if (mIsProcessingBody)
        {
            return mDelegate.read();
        }
        else
        {
            mIsProcessingBody = true;

            // A_24628: The header is used twice. Firstly, it is part of the output, before the
            // encrypted section. Secondly, it is used as additional authentication data, i.e.
            // it is used to update the authentication hash, without being encrypted.
            auto context = mTeeContextProvider();
            auto header = createHeader(context);
            activateEncryption(StreamBuffers::from(header), context);

            return header;
        }
    }
    // GEMREQ-end A_24628-01#doRead

    void TeeEncryptingStreamImplementation::doWrite(StreamBuffers&& plaintextBuffers)
    {
        if (! mIsProcessingBody)
        {
            auto context = mTeeContextProvider();
            auto header = createHeader(context);
            mDelegate.write(StreamBuffers::from(header));
            activateEncryption(std::move(header), context);
            mIsProcessingBody = true;
        }

        mDelegate.write(std::move(plaintextBuffers));
    }

    // GEMREQ-start A_24628-01#createHeader, A_24632#createHeader
    StreamBuffers TeeEncryptingStreamImplementation::createHeader(const Tee3SessionContext& context)
    {
        Assert(context.keyId.bytes.size() == 32);

        // This buffer does not need to be initialized because the following lines will set every
        // byte. Sadly GCC 12 does not recognize this. Disabling the false positive warning
        // only for GCC but not for clang would require quite a few lines of preprocessor lines
        // we give in and initialize the memory.
        uint8_t buffer[TeeStreamFactory::headerSize] = {};
        buffer[0] = context.version;
        buffer[1] = context.isPU ? 1 : 0;
        buffer[2] = static_cast<uint8_t>(context.direction);
        Serialization::uint64::write(context.requestCounter, buffer + 3, buffer + 11);
        std::copy(context.keyId.bytes.data(), context.keyId.bytes.data() + 32, buffer + 11);

        return StreamBuffers::from(reinterpret_cast<const char*>(buffer), sizeof(buffer), false);
    }
    // GEMREQ-end A_24628-01#createHeader, A_24632#createHeader

    // GEMREQ-start A_24619, A_24628-01#activateEncryption, A_24631#activateEncryption, A_24632#activateEncryption
    void TeeEncryptingStreamImplementation::activateEncryption(
        StreamBuffers&& additionalAuthenticationData,
        const Tee3SessionContext& context)
    {
        // The additionalAuthenticationData has to be captured by a lambda which later is
        // duplicated. In order to avoid introducing a copy constructor for StreamBuffers,
        // a shared_ptr for the data is created.
        auto aad = std::make_shared<StreamBuffers>(std::move(additionalAuthenticationData));
        mDelegate = AesGcmStreamCryptor::makeEncryptingStream(
            [key = context.k2Key]() { return key; },
            std::move(mDelegate),
            [counter = context.messageCounter] {
                return AesGcmStreamCryptor::createSemiRandomIV(counter);
            },
            [aad]() mutable { return StreamBuffers::from(*aad); });
    }
    // GEMREQ-end A_24619, A_24628-01#activateEncryption, A_24631#activateEncryption, A_24632#activateEncryption

    TeeDecryptingStreamImplementation::TeeDecryptingStreamImplementation(
        TeeStreamFactory::TeeContextProviderForKeyId contextProvider,
        Stream&& delegate,
        const ReadableOrWritable mode)
      : mTeeContextProvider(std::move(contextProvider)),
        mDelegate(std::move(delegate)),
        mIsProcessingBody(false),
        mHeaderBuffer(StreamBuffer::NotLast),
        mMode(mode),
        mRequestCounter() // Will be set when the value is read from the outer header
    {
        // Note that mDelegate at this point is the original delegate stream. That is because
        // we have to read the outer header, which is not encrypted. When the header has been read,
        // the method activateDecryption() will be called to activate the decryption of the original
        // delegate stream.
    }


    bool TeeDecryptingStreamImplementation::isReadSupported() const
    {
        return mMode == Readable;
    }


    bool TeeDecryptingStreamImplementation::isWriteSupported() const
    {
        return mMode == Writable;
    }

    // GEMREQ-start A_24633#doRead
    StreamBuffers TeeDecryptingStreamImplementation::doRead()
    {
        if (! mIsProcessingBody)
        {
            mIsProcessingBody = true;
            mHeaderBuffer = mDelegate.readExactly(TeeStreamFactory::headerSize);
            processTeeHeader(std::move(mHeaderBuffer));
        }

        // Read some data from the delegate stream. At this point, mDelegate is an AesGcm decrypting
        // stream that is wrapped around the original delegate.
        return mDelegate.read();
    }
    // GEMREQ-end A_24633#doRead

    void TeeDecryptingStreamImplementation::doWrite(StreamBuffers&& buffers)
    {
        if (! mIsProcessingBody)
        {
            // Accumulate the given buffers until we have at least the complete header.
            mHeaderBuffer.pushBack(std::move(buffers));
            if (mHeaderBuffer.size() >= TeeStreamFactory::headerSize)
            {
                mIsProcessingBody = true;
                buffers = mHeaderBuffer.splitOff(TeeStreamFactory::headerSize);
                processTeeHeader(std::move(mHeaderBuffer));
                // don't exit yet, `buffers` have to be forwarded to the decrypting stream.
                // mHeaderBuffer is not used for the reminder of the decryption process.
            }
            else
            {
                // The header is not yet complete, wait for the next doWrite().
                // Until then, nothing is written to the delegate stream.
                return;
            }
        }

        // Write (the remaining part of) buffers to the delegate stream. By now it is an AesGcm
        // encrypting stream that is wrapped around the original delegate.
        mDelegate.write(std::move(buffers));
    }

    // GEMREQ-start A_24478, A_24628-01#processTeeHeader, A_24630, A_24633#processTeeHeader, A_24632
    void TeeDecryptingStreamImplementation::processTeeHeader(StreamBuffers&& buffers)
    {
        try
        {
            Assert(buffers.size() == TeeStreamFactory::headerSize);

            const auto header = BinaryBuffer(buffers.toString());
            const uint8_t* buffer = header.data();

            // Deserialize the TEE header.
            const uint8_t version = buffer[0];
            const uint8_t isPU = buffer[1];
            const uint8_t direction = buffer[2];
            mRequestCounter = Serialization::uint64::read(buffer + 3, buffer + 11);
            auto keyId = KeyId::fromBinaryView(BinaryView(buffer + 11, 32));

            // Look up the associated TEE context.
            auto context = mTeeContextProvider(keyId, mRequestCounter);
            // GEMREQ-end A_24632

            Assert(version == context.version) << "invalid version";
            Assert(isPU == (context.isPU ? 1 : 0))
                << assertion::exceptionType<TeeError>(TeeError::Code::PuNonPuFailure);
            assertDirection(direction, context.direction);
            // GEMREQ-end A_24630
            if (context.direction == Tee3SessionContext::Direction::ServerToClient)
            {
                Assert(mRequestCounter == context.requestCounter)
                    << "response request counter mismatch";
            }

            // Now we have the TEE context and with it the K2 message keys. Activate the decryption.
            // The `buffers` object, which contains the TEE header, is also used as AES GCM additional
            // authentication data.
            activateDecryption(context, std::move(buffers));
            // GEMREQ-end A_24628-01#processTeeHeader, A_24633#processTeeHeader
        }
        catch (const TeeError&)
        {
            throw;
        }
        // GEMREQ-end A_24478
        catch (const std::runtime_error& e)
        {
            // Every exception that is not explicitly specified as a TeeError is forwarded as a
            // generic TEE decoding error.
            throw TeeError(TeeError::Code::DecodingError, e.what());
        }
    }

    // GEMREQ-start A_24630#activateDecryption, A_24633#activateDecryption
    void TeeDecryptingStreamImplementation::activateDecryption(
        const Tee3SessionContext& context,
        StreamBuffers&& additionalAuthenticationData)
    {
        // The additionalAuthenticationData has to be captured by a lambda which later is
        // duplicated. In order to avoid introducing a copy constructor for StreamBuffers,
        // a shared_ptr for the data is created.
        auto aad = std::make_shared<StreamBuffers>(std::move(additionalAuthenticationData));
        mDelegate = AesGcmStreamCryptor::makeDecryptingStream(
            [key = context.k2Key]() -> EncryptionKey { return key; },
            std::move(mDelegate),
            {}, // The message counter validator is a left-over from the older TEE protocol (<3)
            [aad]() mutable { return StreamBuffers::from(*aad); },
            [](const std::exception_ptr e) { // NOLINT
                // Rethrow the exception to detect its type.
                try
                {
                    std::rethrow_exception(e);
                }
                catch (const std::runtime_error& e)
                {
                    // We expect only to be called for exceptions that are thrown while the
                    // AES GCM authentication tag is being processed. All of these are to
                    // be reported as TeeError.
                    throw TeeError(TeeError::Code::GcmDecryptionFailure, e.what());
                }
            });
    }
    // GEMREQ-end A_24630#activateDecryption, A_24633#activateDecryption
} // end of anonymous namespace

} // namespace epa
