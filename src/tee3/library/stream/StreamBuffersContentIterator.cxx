/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/StreamBuffersContentIterator.hxx"

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

ConstStreamBuffersContentIterator::ConstStreamBuffersContentIterator()
  : mBuffers{nullptr},
    mBufferIterator{},
    mBufferDataBegin{nullptr},
    mBufferDataEnd{nullptr},
    mDataPointer{nullptr}
{
}


ConstStreamBuffersContentIterator::ConstStreamBuffersContentIterator(
    const StreamBuffers& buffers,
    StreamBuffers::const_iterator bufferIterator)
  : mBuffers{&buffers},
    mBufferIterator{bufferIterator},
    mBufferDataBegin{nullptr},
    mBufferDataEnd{nullptr},
    mDataPointer{nullptr}
{
    initDataPointer(InitPosition::BEGIN);
}


std::ptrdiff_t ConstStreamBuffersContentIterator::operator-(
    const ConstStreamBuffersContentIterator& other) const
{
    if (mBufferIterator != mBuffers->end() && mBufferIterator == other.mBufferIterator)
    {
        // Both iterators refer to a position within the same buffer.
        return mDataPointer - other.mDataPointer;
    }

    return gsl::narrow_cast<std::ptrdiff_t>(getAbsoluteDataOffset())
           - gsl::narrow_cast<std::ptrdiff_t>(other.getAbsoluteDataOffset());
}


void ConstStreamBuffersContentIterator::initDataPointer(InitPosition initPosition)
{
    if (mBufferIterator == mBuffers->end())
    {
        mBufferDataBegin = nullptr;
        mBufferDataEnd = nullptr;
        mDataPointer = nullptr;
    }
    else
    {
        mBufferDataBegin = mBufferIterator->data();
        mBufferDataEnd = mBufferDataBegin + mBufferIterator->size();
        mDataPointer = initPosition == InitPosition::BEGIN ? mBufferDataBegin : mBufferDataEnd;
    }
}


size_t ConstStreamBuffersContentIterator::getAbsoluteDataOffset() const
{
    return getBufferDataOffset() + static_cast<size_t>(mDataPointer - mBufferDataBegin);
}


size_t ConstStreamBuffersContentIterator::getBufferDataOffset() const
{
    if (mBufferIterator == mBuffers->end())
    {
        return mBuffers->size();
    }

    size_t offset = 0;
    for (auto it = mBuffers->begin(); it != mBuffers->end(); ++it)
    {
        if (it == mBufferIterator)
        {
            return offset;
        }
        offset += it->size();
    }

    return offset;
}

} // namespace epa
