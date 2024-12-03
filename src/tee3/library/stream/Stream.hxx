/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAM_HXX
#define EPA_LIBRARY_STREAM_STREAM_HXX

#include "library/stream/StreamBuffers.hxx"

#include <boost/core/noncopyable.hpp>
#include <iosfwd>
#include <memory>

namespace epa
{

class StreamImplementation;

/**
 * Represent a larger set of bytes, say a HTTP request, that is read from or written to in smaller chunks,
 * so that the whole stream content does not have to be in memory at the same time.
 */
class Stream : private boost::noncopyable
{
public:
    Stream();
    explicit Stream(std::unique_ptr<StreamImplementation> implementation);
    Stream(Stream&& other) noexcept;
    ~Stream();

    Stream& operator=(Stream&& other) noexcept;

    bool isReadSupported() const;
    bool isWriteSupported() const;

    /**
     * Write the given list of buffers to an underlying data sink.
     *
     * @throws std::runtime_error if not any or all the data can be written.
     */
    void write(StreamBuffers&& buffer);

    void notifyEnd();

    /**
     * Read data from an underlying data source.
     *
     * @return an empty StreamBuffers list on eof
     */
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
    StreamBuffers readExactly(const size_t length);

    /**
     * Returns the whole content of the stream in a single set of StreamBuffers.
     */
    StreamBuffers readAll();

    /**
     * Reads and discards any available data. You could also use readAll() for that, but this method uses less memory.
     *
     * @return The number of bytes that are being read and discarded by this call.
     */
    size_t discardAll();

    std::unique_ptr<std::streambuf> releaseToReadableStreamBuf();
    std::unique_ptr<std::streambuf> releaseToWritableStreamBuf();

private:
    std::unique_ptr<StreamImplementation> mImplementation;
};

} // namespace epa

#endif
