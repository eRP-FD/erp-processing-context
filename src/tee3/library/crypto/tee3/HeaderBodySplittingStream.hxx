/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_HEADERBODYSPLITTINGSTREAM_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_HEADERBODYSPLITTINGSTREAM_HXX

#include "library/crypto/tee3/TeeStreamFactory.hxx"
#include "library/stream/Stream.hxx"
#include "library/stream/implementation/StreamImplementation.hxx"

class Header;

namespace epa
{

/**
 * Take a given readable or writable stream, parse the HTTP header and forward only the body
 * to the delegate stream.
 */
class HeaderBodySplittingStreamImplementation : public StreamImplementation
{
public:
    using HeaderConsumer = std::function<void(Header&&)>;

    HeaderBodySplittingStreamImplementation(
        Stream&& delegate,
        std::optional<uint64_t> bodyLimit,
        HeaderConsumer&& headerConsumer,
        ReadableOrWritable readableOrWritable,
        bool isRequest,
        bool readHeaderOnInitialization = false);
    ~HeaderBodySplittingStreamImplementation() override;

    bool isReadSupported() const override;
    bool isWriteSupported() const override;

    class Parser;
    class EndDetector;

protected:
    StreamBuffers doRead() override;
    void doWrite(StreamBuffers&& buffers) override;

private:
    std::unique_ptr<Parser> mParser;
    std::unique_ptr<EndDetector> mEndDetector;
    HeaderConsumer mHeaderConsumer;
    ReadableOrWritable mReadableOrWritable;
    Stream mDelegate;
    bool mIsHeaderFinished;
    StreamBuffers mHeaderBuffers;

    void readHeader();
    void processHeaderBuffers(StreamBuffers&& buffers);
    StreamBuffers processBodyBuffers(StreamBuffers&& buffers);
};

} // namespace epa

#endif
