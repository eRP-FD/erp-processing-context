/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/ReadableStreamImplementationStreamBuf.hxx"

#include <cstring>

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/String.hxx"

namespace epa
{

ReadableStreamImplementationStreamBuf::ReadableStreamImplementationStreamBuf(
    std::unique_ptr<StreamImplementation> implementation)
  : mImplementation(std::move(implementation)),
    mReadBuffers{StreamBuffer::NotLast},
    mReadBufferIterator{mReadBuffers.end()}
{
    if (! mImplementation->isReadSupported())
    {
        throw std::logic_error("reading is not supported");
    }
}


std::streamsize ReadableStreamImplementationStreamBuf::xsgetn(
    char* resultBuffer,
    std::streamsize requestedCount)
{
    std::streamsize bytesRead = 0;
    const char* const resultBufferEnd = resultBuffer + requestedCount;

    while (bytesRead < requestedCount)
    {
        if (! ensureBytesReadable())
        {
            return bytesRead;
        }

        const std::streamsize available = std::distance(gptr(), egptr());
        const std::streamsize count = std::min(available, requestedCount - bytesRead);
        String::copyMemorySafely(
            resultBuffer,
            gsl::narrow<size_t>(resultBufferEnd - resultBuffer),
            gptr(),
            static_cast<size_t>(count));

        gbump(gsl::narrow<int>(count));
        bytesRead += count;
        resultBuffer += count;
    }

    return bytesRead;
}


std::streambuf::int_type ReadableStreamImplementationStreamBuf::underflow()
{
    if (! ensureBytesReadable())
    {
        return traits_type::eof();
    }

    return traits_type::to_int_type(*gptr());
}


bool ReadableStreamImplementationStreamBuf::ensureBytesReadable()
{
    while (gptr() == egptr())
    {
        while (mReadBufferIterator == mReadBuffers.end())
        {
            readNextBuffers();

            if (mReadBuffers.isEmpty() && mReadBuffers.isLast())
            {
                return false;
            }
        }

        // Cast the const away. The streambuf's get area is theoretically writable due to the
        // put-back feature. However, since we don't implement pbackfail() it is never actually
        // written to.
        //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        char* data = const_cast<char*>(mReadBufferIterator->data());
        size_t size = mReadBufferIterator->size();
        setg(data, data, data + size);

        ++mReadBufferIterator;
    }

    return true;
}


void ReadableStreamImplementationStreamBuf::readNextBuffers()
{
    do
    {
        mReadBuffers = mImplementation->read();
    } while (mReadBuffers.isEmpty() && ! mReadBuffers.isLast());

    mReadBufferIterator = mReadBuffers.begin();
}

} // namespace epa
