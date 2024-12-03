/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAMBUFFERS_HXX
#define EPA_LIBRARY_STREAM_STREAMBUFFERS_HXX

#include "library/stream/StreamBuffer.hxx"

#include <cstddef>
#include <list>
#include <string>

namespace epa
{

class ConstStreamBuffersContentIterator;
class WritableStreamBuffers;


/**
 * Ordered container of StreamBuffer objects.
 *
 * A StreamBuffer object represents the concatenated content of its StreamBuffer objects.
 */
class StreamBuffers
{
public:
    static StreamBuffers from(const char* data, std::size_t length, bool isLast);
    static StreamBuffers from(std::string, bool isLast);
    static StreamBuffers from(StreamBuffer buffer);
    static StreamBuffers from(const StreamBuffers& other);

    explicit StreamBuffers(bool isLast);
    StreamBuffers(const StreamBuffers&) = delete;
    StreamBuffers(StreamBuffers&& other) noexcept;
    StreamBuffers& operator=(const StreamBuffers&) = delete;
    StreamBuffers& operator=(StreamBuffers&& other) noexcept;
    ~StreamBuffers() = default;

    using iterator = std::list<StreamBuffer>::iterator;
    iterator begin();
    iterator end();

    using const_iterator = std::list<StreamBuffer>::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
    const StreamBuffer& front();
    const StreamBuffer& back();

    typedef ConstStreamBuffersContentIterator ConstContentIterator;
    ConstContentIterator dataBegin() const;
    ConstContentIterator dataEnd() const;

    bool isEmpty() const;
    std::size_t size() const;

    bool isLast() const;
    void setIsLast(bool isLast);

    /**
     * Read data specified by `offset` and `length` into `buffer`.
     * @param offset does not take `getPositionInStream()` into account.
     *        Use 0 to access the first byte.
     * @throw std::logic_error if the requested section is not
     *        completely covered by StreamBuffer objects.
     */
    void read(char* buffer, std::size_t offset, std::size_t length) const;

    /**
     * Convenience version of the `read` method.
     */
    std::string getText(std::size_t offset, std::size_t length) const;

    /**
     * Convert to writable StreamBuffer objects.
     * Depending on the origin of the data this can be done either with
     * a cheap conversion or an expensive copy.
     *
     * Either way, this is a destructive operation in that the
     * original StreamBuffer objects are not available after this call returns.
     */
    WritableStreamBuffers makeWritable();

    void pushFront(StreamBuffer&& buffer);
    void pushBack(StreamBuffers moreBuffers);
    void pushBack(StreamBuffer&& buffer);

    StreamBuffer popFront();

    StreamBuffer popAllToOneBuffer();

    /**
     * Split the buffer at the given position.
     * Everything before that remains in the called StreamBuffers object.
     * Everything after and including the position is returned in a new StreamBuffers object.
     * The `isLast` flag is moved to the returned, split-off part.
     */
    StreamBuffers splitOff(std::size_t length);

    std::size_t getBufferCount() const;
    const StreamBuffer& getBuffer(std::size_t index) const;

    /**
     * Return the position relative to the underlying stream.
     * It is only updated when popFront() is called.
     */
    constexpr std::size_t getStartPositionInStream() const
    {
        return mStartPositionInStream;
    }
    constexpr std::size_t getEndPositionInStream() const
    {
        return mEndPositionInStream;
    }

    /**
     * Convenience functionality that exists primarily for use in tests or debugging.
     */
    std::string toString() const;

    std::string toShortDebugString() const;
    /**
      * Use only for debugging. The returned string includes the content of the buffers, its length
      * and the length and last bits of the individual buffers.
      */
    std::string toDebugString() const;
    std::string toHexDebugString() const;

    void clear();

protected:
    bool mIsLast;
    std::list<StreamBuffer> mBuffers;

private:
    /**
     * Track the position of the first `StreamBuffer` object in `mBuffers` in the underlying stream.
     * It is updated whenever popFront() is called. However, calls to pushFront() do not change it.
     */
    std::size_t mStartPositionInStream;
    std::size_t mEndPositionInStream;

    /**
     *  In order to speed up retrieval of individual characters
     *  we save index and offset of the buffer that was last accessed by getCharacter.
     *  These values are reset when e.g. a buffer is prepended.
     */
    mutable std::list<StreamBuffer>::iterator mLastBufferIterator;
    mutable std::size_t mLastBufferOffset;

    friend class StreamImplementation;
    void swap(StreamBuffers& other) noexcept;
};


class WritableStreamBuffers : public StreamBuffers
{
public:
private:
    friend class StreamBuffers;
    explicit WritableStreamBuffers(std::vector<WritableStreamBuffer>&& buffers);
};

} // namespace epa

#endif
