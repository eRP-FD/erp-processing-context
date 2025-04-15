/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_IMPLEMENTATION_STREAMIMPLEMENTATION_HXX
#define EPA_LIBRARY_STREAM_IMPLEMENTATION_STREAMIMPLEMENTATION_HXX

#include "library/stream/StreamBuffers.hxx"

#include <cstddef>

namespace epa
{

enum ReadableOrWritable
{
    Readable,
    Writable
};


/**
 * A StreamImplementation object is used as "backend" for both Stream and
 * {Readable,Writable}StreamImplementationStreamBuf and can even be moved from one to the other.
 *
 * This class is designed to be extended by deriving from it.
 * Overwrite doRead or doWrite to support reading or writing respectively.
 * Don't forget to also overwrite isReadSupported or isWriteSupported as both return
 * false by default.
 *
 * When the end of the stream is reached then the `notifyEnd` method
 * is called. This gives a derived class the opportunity to, for example, flush buffers,
 * close files or finish REST calls.
 */
class StreamImplementation
{
public:
    StreamImplementation();
    virtual ~StreamImplementation() = 0;

    StreamBuffers read();
    StreamBuffers readAtMost(const size_t maxLength);
    StreamBuffers readExactly(const size_t length);
    StreamBuffers readAll();
    void write(StreamBuffers&& buffer);

    virtual bool isReadSupported() const;
    virtual bool isWriteSupported() const;

    /// Called when the end of the stream is reached.
    virtual void notifyEnd();

protected:
    virtual StreamBuffers doRead();
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    virtual void doWrite(StreamBuffers&& buffers);

    bool isEof() const;

private:
    /**
     * Buffers that have been read but not yet delivered. For example because readExactly() did not want all of them.
     */
    StreamBuffers mPeek;

    /**
     * Track if any of
     * - a previous doRead() returned a StreamBuffers object with set isLast flag
     * - a previous doWrite() was called with a StreamBuffers object with set isLast flag
     * - or the notifyEnd method has been called.
     * is true.
     */
    bool mIsEOS;

    enum class ReadMode
    {
        AnySize,
        AtMost,
        Exact
    };
    StreamBuffers read(const size_t length, const ReadMode mode);
};


class ReadableStreamImplementation : public StreamImplementation
{
public:
    ~ReadableStreamImplementation() override = default;
    bool isReadSupported() const override
    {
        return true;
    }
};


class WritableStreamImplementation : public StreamImplementation
{
public:
    ~WritableStreamImplementation() override = default;
    bool isWriteSupported() const override
    {
        return true;
    }
};


class ReadWritableStreamImplementation : public StreamImplementation
{
public:
    ~ReadWritableStreamImplementation() override = default;
    bool isReadSupported() const override
    {
        return true;
    }
    bool isWriteSupported() const override
    {
        return true;
    }
};

} // namespace epa

#endif
