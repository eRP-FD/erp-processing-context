/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_STREAM_STREAMBUFFER_HXX
#define EPA_LIBRARY_STREAM_STREAMBUFFER_HXX

#include <boost/core/noncopyable.hpp>
#include <cstddef>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

namespace epa
{

class WritableStreamBuffer;


/**
 * Byte buffer that is by Stream objects to pass data from one stage to another.
 *
 * Data is not interpreted. The use of `char*` is more for convenience than anything else.
 *
 * StreamBuffer objects can be read-only or read-write, depending on the
 * origin of the data they hold. A read-only StreamBuffer object that is made writable via makeWritable
 * therefore has to create a copy of the read-only data that is then read-write.
 */
class StreamBuffer : private boost::noncopyable
{
public:
    static constexpr const size_t DefaultBufferLength = 8192;

    static constexpr const bool IsLast = true;
    static constexpr const bool NotLast = false;

    /**
     * Create a new empty StreamBuffer with uninitialized byte array of specified size.
     * Important: the size of the buffer is 0, only the capacity is set!
     */
    static StreamBuffer from(size_t length, const bool isLast);

    /**
     * Create a new StreamBuffer object from a given byte array.
     */
    static StreamBuffer from(const char* data, size_t length, bool isLast);
    static StreamBuffer from(const std::string& data, bool isLast);
    static StreamBuffer from(std::string&& data, bool isLast);
    static StreamBuffer emptyBuffer(bool isLast);

    /**
     * Create a new StreamBuffer object that references the given byte array.
     *
     * THE REFERENCED DATA MUST BE VALID FOR THE LIFETIME OF THE STREAM!
     */
    static StreamBuffer fromReferencing(const char* data, size_t length, bool isLast);

    StreamBuffer(StreamBuffer&& other) noexcept;
    ~StreamBuffer();

    /**
     * Return a read-only pointer to the first character in the buffer.
     * To get writable access you need to call `makeWritable` to obtain a read-write
     * version of this buffer.
     */
    const char* data() const;

    /**
     * Return the size of the buffer, i.e. the number of characters in it.
     * Note that the last byte is in general *not* a null byte.
     */
    size_t size() const;

    WritableStreamBuffer makeWritable();
    bool isReadOnly() const;

    bool isLast() const;
    void setIsLast(bool isLast);

    std::tuple<StreamBuffer, StreamBuffer> split(size_t length) const;

    std::string_view toString() const;

    /**
     * This may turn out to be too slow.
     * The current implementation is choses robustness and clarity over speed. The assumption is that we
     * don't have to make many requests per application.
     */
    char operator[](size_t index) const;
    char& operator[](size_t index);

    class Data;

protected:
    std::shared_ptr<Data> mData;
    bool mIsLast;
    const bool mIsReadOnly;

    explicit StreamBuffer(bool isReadOnly = true);
    StreamBuffer(std::shared_ptr<Data>&& data, bool isLast, bool isReadOnly);
};


/**
 * A modifiable version of StreamBuffer.
 *
 * The distinction between read-only and read-write is a bit crude at the moment.
 * With more time this could be improved considerably.
 *
 * The important point is that data that is only referred at but is owned by somebody else is not modified.
 * With the current implementation it is possible that this case never comes up, i.e. that external data is always
 * copied into a dedicated and modifiable buffer. Until that is confirmed we need the distinction.
 */
class WritableStreamBuffer : public StreamBuffer
{
public:
    static WritableStreamBuffer createBuffer(size_t requestedLength = DefaultBufferLength);

    /**
     * Return a read-write pointer to the first character in the buffer.
     * If a read-only pointer is enough, consider to use the `data` method in the base class.
     */
    char* writableData();

    size_t capacity() const;

    /**
     * The new size must not be larger than the capacity of the buffer.
     * The purpose of this method is to adapt the size after content has been received by reading from an outside
     * source. Such a read should be capped by the capacity of this WritableStreamBuffer but can be smaller.
     */
    void setSize(size_t newSize);

private:
    friend class StreamBuffer;
    WritableStreamBuffer(std::shared_ptr<Data>&& data, bool isLast);
};

} // namespace epa

#endif
