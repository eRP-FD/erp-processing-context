/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/Stream.hxx"
#include "library/stream/ReadableStreamImplementationStreamBuf.hxx"
#include "library/stream/WritableStreamImplementationStreamBuf.hxx"

#include <stdexcept>

namespace epa
{

Stream::Stream() = default;


Stream::Stream(std::unique_ptr<StreamImplementation> implementation)
  : mImplementation(std::move(implementation))
{
}


Stream::Stream(Stream&& other) noexcept
  : mImplementation(std::move(other.mImplementation))
{
}


Stream::~Stream() = default;


Stream& Stream::operator=(Stream&& other) noexcept
{
    mImplementation = std::move(other.mImplementation);

    return *this;
}


bool Stream::isReadSupported() const
{
    return mImplementation && mImplementation->isReadSupported();
}


bool Stream::isWriteSupported() const
{
    return mImplementation && mImplementation->isWriteSupported();
}


void Stream::write(StreamBuffers&& buffers)
{
    if (! mImplementation)
        throw std::logic_error("Trying to write to invalid Stream");
    mImplementation->write(std::move(buffers));
}


void Stream::notifyEnd()
{
    if (mImplementation)
        mImplementation->notifyEnd();
}


StreamBuffers Stream::read()
{
    if (! mImplementation)
        throw std::logic_error("Trying to read from invalid Stream");
    return mImplementation->read();
}


StreamBuffers Stream::readAtMost(const size_t maxLength)
{
    if (! mImplementation)
        throw std::logic_error("Trying to read from invalid Stream");
    return mImplementation->readAtMost(maxLength);
}


StreamBuffers Stream::readExactly(const size_t length)
{
    if (! mImplementation)
        throw std::logic_error("Trying to read from invalid Stream");
    return mImplementation->readExactly(length);
}


StreamBuffers Stream::readAll()
{
    if (! mImplementation)
        throw std::logic_error("Trying to read from invalid Stream");
    return mImplementation->readAll();
}


size_t Stream::discardAll()
{
    if (! mImplementation)
        return 0;

    StreamBuffers buffers = read();
    size_t bytes = buffers.size();
    while (! buffers.isLast())
    {
        buffers.clear();
        buffers = read();
        bytes += buffers.size();
    }
    return bytes;
}


std::unique_ptr<std::streambuf> Stream::releaseToReadableStreamBuf()
{
    if (! mImplementation)
        throw std::logic_error("Trying to release an invalid Stream to std::streambuf");
    return std::make_unique<ReadableStreamImplementationStreamBuf>(std::move(mImplementation));
}


std::unique_ptr<std::streambuf> Stream::releaseToWritableStreamBuf()
{
    if (! mImplementation)
        throw std::logic_error("Trying to release an invalid Stream to std::streambuf");
    return std::make_unique<WritableStreamImplementationStreamBuf>(std::move(mImplementation));
}

} // namespace epa
