/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/ReadableByteStream.hxx"
#include "library/util/Assert.hxx"

namespace epa
{

ReadableByteStream::ReadableByteStream(Stream&& inputStream)
  : mInputStream(std::move(inputStream)),
    mBuffers(StreamBuffer::NotLast),
    mOffset(0),
    mAvailable(0),
    mAvailableInCurrentBuffer(0),
    mCurrentBuffer(nullptr)
{
}


size_t ReadableByteStream::bufferedSize() const
{
    return mAvailable;
}


uint8_t ReadableByteStream::readByte()
{
    if (mAvailable == 0)
        Assert(! isAtEnd()) << "can't read past end of stream";

    --mAvailableInCurrentBuffer;
    --mAvailable;
    ++mOffset;
    const uint8_t b = *mCurrentBuffer++;
    if (mAvailableInCurrentBuffer == 0)
    {
        mBuffers.popFront();
        setCurrentBuffer();
    }
    return b;
}


StreamBuffers ReadableByteStream::read()
{
    if (mAvailable == 0)
        return mInputStream.read();
    else
    {
        // Discard what has already been read.
        auto result = mBuffers.splitOff(mOffset);
        mBuffers = StreamBuffers(StreamBuffer::NotLast);

        // Update offset and sizes.
        mOffset = 0;
        mAvailable = mBuffers.size();
        setCurrentBuffer();

        return result;
    }
}


StreamBuffers ReadableByteStream::readAtMost(const size_t length)
{
    if (mAvailable == 0)
    {
        // Pass through. No bookkeeping required.
        return mInputStream.readAtMost(length);
    }
    else if (length <= mAvailable)
    {
        // Split off what has already been read.
        auto result = mBuffers.splitOff(mOffset);
        // Keep everything past `length`.
        mBuffers = result.splitOff(length);

        Assert(result.size() == length) << "internal error";

        // Update offset and sizes.
        mOffset = 0;
        mAvailable = mBuffers.size();
        setCurrentBuffer();

        return result;
    }
    else
    {
        // Return all that is currently buffered.
        auto result = mBuffers.splitOff(mOffset);
        mBuffers = StreamBuffers(StreamBuffer::NotLast);
        mOffset = 0;
        mAvailable = 0;
        setCurrentBuffer();
        return result;
    }
}


StreamBuffers ReadableByteStream::readExactly(const size_t count)
{
    if (mAvailable == 0)
    {
        // Pass through. No bookkeeping required.
        return mInputStream.readExactly(count);
    }
    else
    {
        provideBuffer(count);
        return readAtMost(count);
    }
}


bool ReadableByteStream::isAtEnd()
{
    if (mAvailable > 0)
        return false;
    else if (mBuffers.isLast())
        return true;
    else
    {
        provideBuffer(1);
        return mAvailable == 0;
    }
}


void ReadableByteStream::provideBuffer(const size_t requestedLength)
{
    while (! mBuffers.isLast() && mBuffers.size() < requestedLength)
    {
        if (mAvailable == 0)
        {
            mBuffers = mInputStream.read();
            mAvailable = mBuffers.size();
            setCurrentBuffer();
        }
        else
        {
            auto buffers = mInputStream.read();
            mAvailable += buffers.size();
            mBuffers.pushBack(std::move(buffers));
        }
    }
}


void ReadableByteStream::setCurrentBuffer()
{
    if (mAvailable > 0)
    {
        const auto& buffer = mBuffers.getBuffer(0);
        mCurrentBuffer = reinterpret_cast<const uint8_t*>(buffer.data());
        mAvailableInCurrentBuffer = buffer.size();
    }
    else
    {
        mCurrentBuffer = nullptr;
        mAvailableInCurrentBuffer = 0;
    }
    mOffset = 0;
}

} // namespace epa
