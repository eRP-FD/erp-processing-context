/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAMBUFFERSCONTENTITERATOR_HXX
#define EPA_LIBRARY_STREAM_STREAMBUFFERSCONTENTITERATOR_HXX

#include "library/stream/StreamBuffers.hxx"

namespace epa
{

class ConstStreamBuffersContentIterator
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = char;
    using pointer = const char*;
    using reference = const char&;

    ConstStreamBuffersContentIterator();

    ConstStreamBuffersContentIterator(
        const StreamBuffers& buffers,
        StreamBuffers::const_iterator bufferIterator);

    reference operator*() const;

    pointer operator->();

    ConstStreamBuffersContentIterator& operator++();
    const ConstStreamBuffersContentIterator operator++(int);

    ConstStreamBuffersContentIterator& operator--();
    const ConstStreamBuffersContentIterator operator--(int);

    std::ptrdiff_t operator-(const ConstStreamBuffersContentIterator& other) const;

    bool operator==(const ConstStreamBuffersContentIterator& other) const;
    bool operator!=(const ConstStreamBuffersContentIterator& other) const;

private:
    enum class InitPosition
    {
        BEGIN,
        END,
    };

    void initDataPointer(InitPosition initPosition);

    size_t getAbsoluteDataOffset() const;
    size_t getBufferDataOffset() const;

    const StreamBuffers* mBuffers;
    StreamBuffers::const_iterator mBufferIterator;
    const char* mBufferDataBegin;
    const char* mBufferDataEnd;
    const char* mDataPointer;
};


inline ConstStreamBuffersContentIterator::reference ConstStreamBuffersContentIterator::operator*()
    const
{
    return *mDataPointer;
}


inline ConstStreamBuffersContentIterator::pointer ConstStreamBuffersContentIterator::operator->()
{
    return mDataPointer;
}


inline ConstStreamBuffersContentIterator& ConstStreamBuffersContentIterator::operator++()
{
    mDataPointer++;
    while (mDataPointer == mBufferDataEnd && mDataPointer != nullptr)
    {
        ++mBufferIterator;
        initDataPointer(InitPosition::BEGIN);
    }
    return *this;
}


//NOLINTNEXTLINE(readability-const-return-type)
inline const ConstStreamBuffersContentIterator ConstStreamBuffersContentIterator::operator++(int)
{
    ConstStreamBuffersContentIterator tmp = *this;
    operator++();
    return tmp;
}


inline ConstStreamBuffersContentIterator& ConstStreamBuffersContentIterator::operator--()
{
    while (mDataPointer == mBufferDataBegin)
    {
        --mBufferIterator;
        initDataPointer(InitPosition::END);
    }
    mDataPointer--;
    return *this;
}

//NOLINTNEXTLINE(readability-const-return-type)
inline const ConstStreamBuffersContentIterator ConstStreamBuffersContentIterator::operator--(int)
{
    ConstStreamBuffersContentIterator tmp = *this;
    operator--();
    return tmp;
}


inline bool ConstStreamBuffersContentIterator::operator==(
    const ConstStreamBuffersContentIterator& other) const
{
    return mBufferIterator == other.mBufferIterator && mDataPointer == other.mDataPointer;
}


inline bool ConstStreamBuffersContentIterator::operator!=(
    const ConstStreamBuffersContentIterator& other) const
{
    return ! (*this == other);
}

} // namespace epa

#endif
