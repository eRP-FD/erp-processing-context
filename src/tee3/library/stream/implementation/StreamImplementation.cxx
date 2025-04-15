/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/implementation/StreamImplementation.hxx"
#include "library/log/Logging.hxx"
#include "library/util/Assert.hxx"

#include <cstddef>
#include <stdexcept>

namespace epa
{

StreamImplementation::StreamImplementation()
  : mPeek(StreamBuffer::NotLast),
    mIsEOS(false)
{
}


StreamImplementation::~StreamImplementation() = default;


StreamBuffers StreamImplementation::read()
{
    return read(0, ReadMode::AnySize);
}


StreamBuffers StreamImplementation::readAtMost(const size_t maxLength)
{
    return read(maxLength, ReadMode::AtMost);
}


StreamBuffers StreamImplementation::readExactly(const size_t length)
{
    return read(length, ReadMode::Exact);
}


StreamBuffers StreamImplementation::readAll()
{
    StreamBuffers buffers = read();
    while (! buffers.isLast())
        buffers.pushBack(read());
    return buffers;
}

// GEMREQ-start A_24633#readExactly
StreamBuffers StreamImplementation::read(const size_t length, const ReadMode mode)
{
    if (! isReadSupported())
        throw std::logic_error("stream does not support read");
    else if (mIsEOS)
        return StreamBuffers(StreamBuffer::IsLast);

    // Get the initial set of buffers.
    StreamBuffers buffers(StreamBuffer::NotLast);
    if (! mPeek.isEmpty())
        buffers.swap(mPeek);
    else
        buffers = doRead();

    while (buffers.size() < length && mode == ReadMode::Exact)
    {
        // Read additional data until we have at least the required number.
        if (buffers.isLast())
        {
            const std::string message = "stream too short, available "
                                        + std::to_string(buffers.size()) + ", expected "
                                        + std::to_string(length);
            LOG(ERROR) << "ERROR: " << message;
            throw std::runtime_error(message);
        }
        buffers.pushBack(doRead());
    }
    // GEMREQ-end A_24633#readExactly

    if (buffers.size() > length && (mode == ReadMode::AtMost || mode == ReadMode::Exact))
    {
        // We got more bytes than where requested. Split of the excess bytes and store them in mPeek.
        mPeek = buffers.splitOff(length);
        Assert(buffers.size() == length);
    }

    if (buffers.isLast())
    {
        mIsEOS = true;
        notifyEnd();
    }

    return buffers;
}


void StreamImplementation::write(StreamBuffers&& buffers)
{
    if (mIsEOS)
    {
        Assert(buffers.size() == 0 && buffers.isLast())
            << assertion::exceptionType<std::logic_error>()
            << "write to stream after its end was signalled";
        return; // Multiple "notifyEnd"s are not an error.
    }

    if (! isWriteSupported())
        throw std::logic_error("stream does not support write");

    const bool isLast = buffers.isLast();
    doWrite(std::move(buffers));
    if (isLast)
        notifyEnd();
}


bool StreamImplementation::isReadSupported() const
{
    return false;
}


bool StreamImplementation::isWriteSupported() const
{
    return false;
}


StreamBuffers StreamImplementation::doRead()
{
    return StreamBuffers(StreamBuffer::IsLast);
}


// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void StreamImplementation::doWrite(StreamBuffers&&)
{
    // This class is designed to be extended by deriving from it,
    // the default implementation is a no-op implementation, nothing is done.
}


bool StreamImplementation::isEof() const
{
    return mIsEOS;
}


void StreamImplementation::notifyEnd()
{
    mIsEOS = true;
}
} // namespace epa
