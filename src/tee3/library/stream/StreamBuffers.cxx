/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/stream/StreamBuffers.hxx"
#include "library/stream/StreamBuffersContentIterator.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "shared/util/String.hxx"

namespace epa
{

// ===== StreamBuffers =======================================================

StreamBuffers StreamBuffers::from(const char* data, const std::size_t length, const bool isLast)
{
    return StreamBuffers::from(StreamBuffer::from(data, length, isLast));
}


StreamBuffers StreamBuffers::from(std::string text, const bool isLast)
{
    return from(StreamBuffer::from(std::move(text), isLast));
}


StreamBuffers StreamBuffers::from(StreamBuffer buffer)
{
    StreamBuffers buffers(buffer.isLast());
    buffers.pushBack(std::move(buffer));
    return buffers;
}


StreamBuffers StreamBuffers::from(const StreamBuffers& other)
{
    StreamBuffers clone(other.isLast());
    for (const auto& buffer : other.mBuffers)
        clone.pushBack(StreamBuffer::from(buffer.data(), buffer.size(), buffer.isLast()));
    return clone;
}


StreamBuffers::StreamBuffers(const bool isLast)
  : mIsLast(isLast),
    mBuffers(),
    mStartPositionInStream(0),
    mEndPositionInStream(0),
    mLastBufferIterator(mBuffers.begin()),
    mLastBufferOffset(0)
{
}


StreamBuffers::StreamBuffers(StreamBuffers&& other) noexcept
  : mIsLast(other.mIsLast),
    mBuffers(std::move(other.mBuffers)),
    mStartPositionInStream(other.mStartPositionInStream),
    mEndPositionInStream(other.mEndPositionInStream),
    mLastBufferIterator(mBuffers.begin()),
    mLastBufferOffset(0)
{
    other.mStartPositionInStream = 0;
    other.mEndPositionInStream = 0;
}


StreamBuffers& StreamBuffers::operator=(StreamBuffers&& other) noexcept
{
    mIsLast = other.mIsLast;
    mBuffers.swap(other.mBuffers);
    mStartPositionInStream = other.mStartPositionInStream;
    mEndPositionInStream = other.mEndPositionInStream;

    mLastBufferIterator = mBuffers.begin();
    mLastBufferOffset = 0;

    other.mStartPositionInStream = 0;
    other.mEndPositionInStream = 0;

    return *this;
}


void StreamBuffers::swap(StreamBuffers& other) noexcept
{
    mBuffers.swap(other.mBuffers);
    std::swap(mIsLast, other.mIsLast);
    std::swap(mStartPositionInStream, other.mStartPositionInStream);
    std::swap(mEndPositionInStream, other.mEndPositionInStream);
    std::swap(mLastBufferIterator, other.mLastBufferIterator);
    std::swap(mLastBufferOffset, other.mLastBufferOffset);
}


StreamBuffers::iterator StreamBuffers::begin()
{
    return mBuffers.begin();
}


StreamBuffers::iterator StreamBuffers::end()
{
    return mBuffers.end();
}


StreamBuffers::const_iterator StreamBuffers::begin() const
{
    return mBuffers.begin();
}


StreamBuffers::const_iterator StreamBuffers::end() const
{
    return mBuffers.end();
}


const StreamBuffer& StreamBuffers::front()
{
    Assert(! mBuffers.empty());
    return mBuffers.front();
}


const StreamBuffer& StreamBuffers::back()
{
    Assert(! mBuffers.empty());
    return mBuffers.back();
}


StreamBuffers::ConstContentIterator StreamBuffers::dataBegin() const
{
    return StreamBuffers::ConstContentIterator{*this, begin()};
}


StreamBuffers::ConstContentIterator StreamBuffers::dataEnd() const
{
    return StreamBuffers::ConstContentIterator{*this, end()};
}


bool StreamBuffers::isEmpty() const
{
    return mBuffers.empty();
}


std::size_t StreamBuffers::size() const
{
    return mEndPositionInStream - mStartPositionInStream;
}


bool StreamBuffers::isLast() const
{
    return mIsLast;
}


void StreamBuffers::setIsLast(const bool isLast)
{
    mIsLast = isLast;
    if (! mBuffers.empty())
        mBuffers.back().setIsLast(isLast);
}


void StreamBuffers::read(char* buffer, std::size_t offset, std::size_t length) const
{
    Assert(offset <= mEndPositionInStream);
    Assert(offset + length <= mEndPositionInStream);

    for (const auto& streamBuffer : mBuffers)
    {
        if (offset < streamBuffer.size())
        {
            if (length == 0)
                break;

            const std::size_t count = std::min(length, streamBuffer.size() - offset);
            std::copy(streamBuffer.data() + offset, streamBuffer.data() + offset + count, buffer);

            length -= count;
            buffer += count;
        }
        if (offset > streamBuffer.size())
            offset -= streamBuffer.size();
        else
            offset = 0;
    }

    if (length > 0)
        throw std::logic_error("StreamBuffers is shorter than caller expected");
}


std::string StreamBuffers::getText(std::size_t offset, std::size_t length) const
{
    std::string result(length, ' ');
    read(result.data(), offset, length);
    return result;
}


WritableStreamBuffers StreamBuffers::makeWritable()
{
    std::vector<WritableStreamBuffer> writableBuffers;
    writableBuffers.reserve(mBuffers.size());
    for (auto&& buffer : mBuffers)
        writableBuffers.push_back(buffer.makeWritable());
    return WritableStreamBuffers(std::move(writableBuffers));
}


void StreamBuffers::pushFront(StreamBuffer&& buffer)
{
    if (buffer.size() == 0)
        return;

    mEndPositionInStream += buffer.size();
    buffer.setIsLast(mBuffers.empty() && mIsLast);
    mBuffers.emplace_front(std::move(buffer));
    mLastBufferIterator = mBuffers.begin();
    mLastBufferOffset = 0;
}


void StreamBuffers::pushBack(StreamBuffers moreBuffers)
{
    const bool wasEmpty = mBuffers.empty();
    if (! wasEmpty)
        mBuffers.back().setIsLast(false);

    mEndPositionInStream += moreBuffers.size();
    mBuffers.splice(mBuffers.end(), moreBuffers.mBuffers);

    setIsLast(moreBuffers.isLast());

    if (wasEmpty)
        mLastBufferIterator = mBuffers.begin();
}


void StreamBuffers::pushBack(StreamBuffer&& buffer)
{
    // Do not append empty buffers ...
    if (buffer.size() == 0)
    {
        // ... but use their isLast flag.
        if (buffer.isLast())
        {
            mIsLast = true;
        }
        return;
    }

    if (! mBuffers.empty())
    {
        mBuffers.back().setIsLast(false);
    }
    mIsLast = buffer.isLast();
    mEndPositionInStream += buffer.size();
    mBuffers.push_back(std::move(buffer));

    if (mBuffers.size() == 1)
        mLastBufferIterator = mBuffers.begin();
}


StreamBuffer StreamBuffers::popFront()
{
    if (mBuffers.empty())
        throw std::logic_error("buffers is empty => no buffer to return");

    StreamBuffer result = std::move(mBuffers.front());
    mBuffers.pop_front();
    mStartPositionInStream += result.size();

    mLastBufferIterator = mBuffers.begin();
    mLastBufferOffset = 0;

    return result;
}


StreamBuffer StreamBuffers::popAllToOneBuffer()
{
    WritableStreamBuffer concatenatingBuffer = StreamBuffer::from(size(), isLast()).makeWritable();
    std::size_t totalWritten = 0;
    bool wasLast = false;
    for (auto& buffer : mBuffers)
    {
        Assert(! wasLast);
        String::copyMemorySafely(
            concatenatingBuffer.writableData() + totalWritten,
            concatenatingBuffer.capacity() - totalWritten,
            buffer.data(),
            buffer.size());
        totalWritten += buffer.size();
        wasLast = wasLast || buffer.isLast();
    }
    Assert(totalWritten == size());
    concatenatingBuffer.setSize(totalWritten);

    clear();

    return concatenatingBuffer;
}


void StreamBuffers::clear()
{
    mBuffers.clear();
    mStartPositionInStream = 0;
    mEndPositionInStream = 0;
    mLastBufferIterator = mBuffers.begin();
    mLastBufferOffset = 0;
}


StreamBuffers StreamBuffers::splitOff(const std::size_t length)
{
    Assert(length <= size());

    // Handle the trivial case.
    if (length == size())
        return StreamBuffers(mIsLast);

    std::list<StreamBuffer> buffersA;
    std::list<StreamBuffer> buffersB;
    std::size_t sizeA = 0;

    // Distribute the buffers in mBuffers to two sets.
    // One buffer may have to be split so that its first part goes at the end of buffersA
    // and its second half to the beginning of buffersB.
    for (auto&& buffer : mBuffers)
    {
        if (sizeA >= length)
            buffersB.emplace_back(std::move(buffer));
        else if (sizeA + buffer.size() <= length)
        {
            sizeA += buffer.size();
            buffersA.emplace_back(std::move(buffer));
        }
        else
        {
            auto [bufferA, bufferB] = buffer.split(length - sizeA);
            sizeA += bufferA.size();
            buffersA.emplace_back(std::move(bufferA));
            buffersB.emplace_back(std::move(bufferB));
        }
    }

    // Set up the StreamBuffers objects: this one and the one that is returned.
    const std::size_t totalSize = size();
    mBuffers.swap(buffersA);
    StreamBuffers result(mIsLast);
    result.mBuffers.swap(buffersB);

    // Update the start and end positions relative to the underlying stream.
    result.mEndPositionInStream = mEndPositionInStream;
    mEndPositionInStream = mStartPositionInStream + length;
    result.mStartPositionInStream = mEndPositionInStream;

    Assert(size() == length);
    Assert(result.size() == totalSize - length);

    mIsLast = false;

    mLastBufferIterator = mBuffers.begin();
    mLastBufferOffset = 0;

    return result;
}


std::size_t StreamBuffers::getBufferCount() const
{
    return mBuffers.size();
}


const StreamBuffer& StreamBuffers::getBuffer(std::size_t index) const
{
    auto iterator = mBuffers.begin();
    while (index > 0)
    {
        --index;
        ++iterator;
    }
    return *iterator;
}


std::string StreamBuffers::toString() const
{
    std::string s;
    s.reserve(size());
    for (const auto& buffer : mBuffers)
        s.append(buffer.data(), buffer.size());
    return s;
}


std::string StreamBuffers::toShortDebugString() const
{
    std::ostringstream s{};
    s << "content has total size " << size() << " in " << mBuffers.size()
      << " buffers and individual sizes/last bits";
    for (const auto& buffer : mBuffers)
        s << ' ' << buffer.size() << '/' << buffer.isLast();
    return s.str();
}


std::string StreamBuffers::toDebugString() const
{
    std::ostringstream s{};
    s << "content is '";
    for (const auto& buffer : mBuffers)
        s << std::string_view(buffer.data(), buffer.size());
    s << "' with total size " << size() << " in " << mBuffers.size()
      << " buffers and individual sizes/last bits";
    for (const auto& buffer : mBuffers)
        s << ' ' << buffer.size() << '/' << buffer.isLast();
    return s.str();
}


std::string StreamBuffers::toHexDebugString() const
{
    std::ostringstream s{};
    s << "content is '";
    for (const auto& buffer : mBuffers)
        s << ByteHelper::toHex(std::string_view(buffer.data(), buffer.size()));
    s << "' with total size " << size() << " in " << mBuffers.size()
      << " buffers and individual sizes/last bits";
    for (const auto& buffer : mBuffers)
        s << ' ' << buffer.size() << '/' << buffer.isLast();
    return s.str();
}


// ===== WritableStreamBuffers ===============================================

WritableStreamBuffers::WritableStreamBuffers(std::vector<WritableStreamBuffer>&& writableBuffers)
  : StreamBuffers(writableBuffers.empty() || writableBuffers[writableBuffers.size() - 1].isLast())
{
    for (auto&& writableBuffer : std::move(writableBuffers))
        pushBack(writableBuffer.makeWritable());
}

} // namespace epa
