/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBORWRITER_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBORWRITER_HXX

#include "library/util/BinaryBuffer.hxx"
#include "library/util/cbor/Cbor.hxx"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stack>
#include <string>
#include <tuple>

namespace epa
{

class ReadableByteStream;
class Stream;
class StreamBuffers;


/**
 * The CborWriter writes low-level CBOR items into a stream.
 *
 * For higher-level support of serializing C++ structures, see CborSerializer.
 */
class CborWriter : private boost::noncopyable
{
public:
    explicit CborWriter(Stream& output);

    /**
     * The destructor, among other things, flushes any remaining bytes from a local buffer into
     * the stream. In case you want to use the stream's content before the destructor is called,
     * you have to call `flushBuffer()` explicitly.
     */
    ~CborWriter();

    /**
     * Write the given `majorType` and the given `value` as a one to five byte sequence.
     */
    void writeMajorType(const Cbor::MajorType majorType, const uint64_t value);

    /**
     * Write the given `majorType` without explicit size but with the `indefinite` flag
     * as a single byte to the stream.
     */
    void writeMajorTypeIndefinite(const Cbor::MajorType majorType);

    /**
     * Write a zero (apart from the item head) byte "simple" value, i.e. majorType==7 and simple != 24.
     */
    void writeSimple(Cbor::Simple simpleValue);

    /**
     * Write a break byte (majorType==7, simple == 31), i.e. 0xff.
     */
    void writeBreak();

    /**
     * Write the given data as-is to the stream.
     */
    void writeBytes(const uint8_t* data, size_t count);

    /**
     * Write the content of the given `buffers` as-is to the stream.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void writeBuffers(StreamBuffers&& buffers);

    /**
     * Write a single byte to the stream.
     */
    void writeUint8(uint8_t value);

    /**
     * Write a 16 bit integer as 2 bytes to the stream.
     */
    void writeUint16(uint16_t value);

    /**
     * Write a 32 bit integer as 4 bytes to the stream.
     */
    void writeUint32(uint32_t value);

    /**
     * Write a 64 bit integer as 8 bytes to the stream.
     */
    void writeUint64(uint64_t value);

    /**
     * Flush the internal buffer to the stream. Called by the destructor.
     * The IsLast bit of the flushed data is set according to `isLast`. When `isLast` is `true`
     * but there is no data to flush, then an empty buffer is written.
     */
    void flushBuffer(bool isLast = false);

private:
    Stream& mOutput;
    std::array<uint8_t, 32> mBuffer;
    /**
     * How much of `mBuffer` is currently in use, i.e. `mUsed` is like size(), while mBuffer.size()
     * is like capacity(). This different meaninig of size() is the reason why this member is
     * not called `mSize`.
     */
    size_t mUsed;
};

} // namespace epa

#endif
