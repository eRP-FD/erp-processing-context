/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/WritableStreamImplementationStreamBuf.hxx"

#include <cstring>

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/String.hxx"

namespace epa
{

// NOLINTNEXTLINE(*-member-init) - no need to initialize mBuffer
WritableStreamImplementationStreamBuf::WritableStreamImplementationStreamBuf(
    std::unique_ptr<StreamImplementation> implementation)
  : mImplementation(std::move(implementation))
{
    setp(mBuffer, mBuffer + sizeof(mBuffer));
}


std::streamsize WritableStreamImplementationStreamBuf::xsputn(
    const char* buffer,
    const std::streamsize length)
{
    if (! mImplementation->isWriteSupported())
        throw std::logic_error("writing is not supported");

    auto remaining = static_cast<size_t>(length);
    while (remaining > 0)
    {
        // Make sure that we can write some bytes to the local buffer.
        if (epptr() == pptr())
            if (! writeBuffer())
                return -1;

        // Write some bytes to the local buffer.
        const auto available = static_cast<size_t>(std::distance(pptr(), epptr()));
        const size_t count = std::min(remaining, available);

        String::copyMemorySafely(pptr(), static_cast<size_t>(epptr() - pptr()), buffer, count);

        // Bookkeeping
        remaining -= count;
        buffer += count;

        // Advance the write marker.
        pbump(gsl::narrow<int>(count));
    }

    // Write the buffer to work around missing flush.
    writeBuffer();

    return length;
}


int WritableStreamImplementationStreamBuf::overflow(const int intc)
{
    if (! mImplementation->isWriteSupported())
        throw std::logic_error("writing is not supported");

    if (! writeBuffer())
        return EOF;
    else
    {
        if (intc != EOF)
            *mBuffer = static_cast<char>(intc);
        pbump(1);
        return 0;
    }
}


int WritableStreamImplementationStreamBuf::sync()
{
    if (mImplementation->isWriteSupported())
        if (! writeBuffer())
            return -1;
    return 0;
}


bool WritableStreamImplementationStreamBuf::writeBuffer()
{
    if (! mImplementation->isWriteSupported())
        throw std::logic_error("writing is not supported");

    const auto remaining = static_cast<size_t>(std::distance(pbase(), pptr()));
    mImplementation->write(StreamBuffers::from(pbase(), remaining, StreamBuffer::NotLast));
    setp(mBuffer, mBuffer + sizeof(mBuffer));
    return true;
}

} // namespace epa
