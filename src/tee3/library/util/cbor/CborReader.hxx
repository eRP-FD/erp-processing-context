/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBORREADER_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBORREADER_HXX

#include "library/stream/ReadableByteStream.hxx"
#include "library/stream/StreamBuffers.hxx"
#include "library/stream/StreamFactory.hxx"
#include "library/util/cbor/Cbor.hxx"

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>

namespace epa
{

class Stream;
class StreamBuffers;


/**
 * The CborReader is dedicated to reading individual CBOR items from a stream.
 *
 * The CborDeserializer provides a higher-level interface to deserialize a CBOR stream
 * into a C++ object.
 */
class CborReader
{
public:
    constexpr static size_t defaultMaxStringLength = 4096;
    explicit CborReader(ReadableByteStream&& input, size_t maxLength = defaultMaxStringLength);
    explicit CborReader(Stream&& input, size_t maxLength = defaultMaxStringLength);
    explicit CborReader(std::string&& input, size_t maxLength = defaultMaxStringLength);
    explicit CborReader(const BinaryBuffer& input, size_t maxLength = defaultMaxStringLength);

    /**
     * Read all items in the `input` stream, including nested arrays and maps, and call
     * the `callback` for each of them.
     * The `path` keeps track of which item is given to the callback and `type` and `data`
     * help the callback to decide how to process the item.
     * Note that for indefinite strings as well as for large definite strings (i.e.
     * larger than the `maxLength`) the callback can be called multiple times with the same
     * `path` value. In this case, concatenate the individual `data` objects to assemble the
     * string.
     */
    using Callback =
        std::function<void(std::string_view path, Cbor::Type type, bool isLast, Cbor::Data data)>;
    void read(Callback callback);

    /**
     * Read one byte with major type and info value and zero to four bytes of length/numerical value.
     * In case of strings, the string data is not read.
     */
    Cbor::ItemHead parseItemHead();

    /**
     * The `stream()` can be used to read large values (byte string and text strings) that are not
     * contained in the item head.
     */
    ReadableByteStream& stream();

    /**
     * Return the `maxLength` value that was given to the constructor.
     */
    size_t maxLength() const;


    /**
     * Do not instantiate this class directly. Rather, use either ByteStringReader or TextStringReader.
     */
    class StringReaderBase
    {
    public:
        explicit StringReaderBase(CborReader& input, Cbor::Type type);

        StreamBuffers read();

        /**
         * For smaller byte strings this convenience method reads and returns the whole string.
         */
        std::string readContent();

    private:
        CborReader& mReader;
        const Cbor::Type mType;
        size_t mRemaining;
        bool mIsDefinite;
        bool mIsFinished;
    };

    /**
     * The ByteStringReader can be used to read and assemble strings that consist
     * of multiple individual items or synthetic items (where one item has been split up so that
     * each part is not larger than `maxLength()`).
     */
    class ByteStringReader : public StringReaderBase
    {
    public:
        explicit ByteStringReader(CborReader& input);
    };

    /**
     * Differs from ByteStringReader only in the expected item head types. The behavior of
     * reading and assembling individual string parts is the same.
     */
    class TextStringReader : public StringReaderBase
    {
    public:
        explicit TextStringReader(CborReader& input);
    };

    enum ToStringMode
    {
        EndOfStream,
        ItemCount,
        Break
    };
    /**
     * Transform a (part of a) CBOR serialization into a (somewhat) human-readable string representation.
     *
     * Intended use cases:
     * - serialization of a complex map key
     * - debugging
     * - testing
     */
    std::string toString(ToStringMode mode = ToStringMode::EndOfStream, size_t itemCount = 0);
    /**
     * Use this variant only when you have already parsed the item head of the first item
     * and can't push the byte back into the stream.
     */
    std::string toString(
        const Cbor::ItemHead& initialItemHead,
        ToStringMode mode = ToStringMode::EndOfStream,
        size_t itemCount = 0);

private:
    ReadableByteStream mInput;
    const size_t mMaxLength;

    /**
     * Return the argument that is defined by the info field, i.e. the lower 5 bits of a
     * cbor item descriptor. Up to eight additional bytes may be read from the input.
     */
    uint64_t readInfoLength(const Cbor::Info info);

    /**
     * The following readUint# methods read 1, 2, 4 or 8 bytes respectively, orders them
     * according to the CBOR specification and the C++ integer byte order and
     * finally assembles them into the requested integer type.
     *
     * Note that these functions are called for both unsigned and negative numbers: CBOR's
     * numbers are not stored in two's complement form but with a separate sign bit (in form
     * of its own CBOR major type).
     */
    uint8_t readUint8();
    uint16_t readUint16();
    uint32_t readUint32();
    uint64_t readUint64();

    /**
     * Read a CBOR object. One object can be a single item, like an integer, a collection of items,
     * like for an indefinite or large string, or an array or map.
     * The callback will be called one or more times.
     */
    Cbor::ItemHead readValue(Callback& callback, const std::string& path);

    /**
     * The outer part of a cbor data structure behaves like an implicit, indefinite array:
     * Objects of different types until the stream ends.
     */
    void readPlain(Callback& callback, const std::string& path);

    /**
     * Read all items from a definite or indefinite array. The final part of the path that
     * is given to the callback is of the form [#], 0-based.
     */
    void readArray(Callback& callback, const std::string& path, size_t itemCount);

    /**
     * Read all key-value pairs from a definite or indefinite map. The key is given to the
     * callback as last part of the path.
     */
    void readMap(Callback& callback, const std::string& path, size_t pairCount);

    /**
     * Read a byte or text string. The callback is called one or more times, depending
     * on whether the string is indefinite or larger than `maxLength()`.
     */
    void readString(Callback& callback, const std::string& path, Cbor::ItemHead itemHead);
};

} // namespace epa

#endif
