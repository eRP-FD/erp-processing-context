/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_READABLEBYTESTREAM_HXX
#define EPA_LIBRARY_STREAM_READABLEBYTESTREAM_HXX

#include "library/stream/Stream.hxx"

namespace epa
{

/**
 * A readable `Stream` which supports reading byte by byte but which also retains (most)
 * of the read* methods af a `Stream`.
 */
class ReadableByteStream
{
public:
    explicit ReadableByteStream(Stream&& inputStream);

    /**
     * Consume and return a single byte from the stream. If necessary new StreamBuffers will
     * be read from the input stream.
     * @throws if the stream end has been reached before
     */
    uint8_t readByte();

    StreamBuffers read();
    /**
     * Use this variant if you want to fill a given buffer that has maxLength available space
     * but are not prepared to buffer a possibly larger StreamBuffers object.
     */
    StreamBuffers readAtMost(const size_t maxLength);
    /**
     * Use this variant if you need a specific number of bytes.
     * @throws if the requested number of bytes can not be provided.
     */
    StreamBuffers readExactly(const size_t count);

    /**
     * Returns if the stream has been read to its end.
     */
    bool isAtEnd();

    /**
     * The number of unread bytes that are available in memory.
     */
    size_t bufferedSize() const;

private:
    Stream mInputStream;
    StreamBuffers mBuffers;
    size_t mOffset;
    size_t mAvailable;
    size_t mAvailableInCurrentBuffer;
    const uint8_t* mCurrentBuffer;

#ifdef FRIEND_TEST
    FRIEND_TEST(ReadableByteStreamTest, isAtEnd);
    FRIEND_TEST(ReadableByteStreamTest, readByte);
#endif

    /**
     * Make sure that `mAvailable >= requestedLength` by appending buffers to `mBuffers` as
     * necessary.
     */
    void provideBuffer(const size_t requestedLength);
    void setCurrentBuffer();
};

} // namespace epa

#endif
