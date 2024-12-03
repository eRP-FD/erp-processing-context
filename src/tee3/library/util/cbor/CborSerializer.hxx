/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBORSERIALIZER_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBORSERIALIZER_HXX

#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/stream/StreamFactory.hxx"
#include "library/util/Assert.hxx"
#include "library/util/BinaryBuffer.hxx"
#include "library/util/cbor/Cbor.hxx"
#include "library/util/cbor/CborWriter.hxx"
#include "library/wrappers/GLog.hxx"

#include <any>
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
 * A simple CBOR writer. Streaming is supported only for byte strings when they
 * are given as StreamBuffer objects.
 *
 * Note that you have to call `flushBuffer()` to finish writing a CBOR structure so that all bytes
 * in the small internal buffer are flushed out to the underlying stream.
 */
class CborSerializer : private boost::noncopyable
{
public:
    explicit CborSerializer(Stream& out);
    ~CborSerializer();

    /**
     * Write `value` as unsigned integer to the underlying stream.
     * The serializer will automatically use the shortest representation.
     */
    CborSerializer& unsignedInteger(uint64_t value);

    /**
     * Write `value` as negative integer.
     * The serializer will automatically use the shortest representation.
     *
     * Note that `value` is still expected to be negative. This wastes the highest bit.
     * This is so that the caller does not have to deal with a double negative and can express `value`
     * with its true value.
     *
     * If the highest bit of a signed 64 bit number should ever be an issue, we can provide
     * a special interface for it.
     *
     * @throws if `value` is 0 or positive.
     */
    CborSerializer& negativeInteger(int64_t value);

    /**
     * Convenience version of `unsignedInteger()` and `negativeInteger()` when you have no prior
     * knowledge about signedness of `value` and don't want to clutter the calling code with
     * the required if/else statement.
     */
    CborSerializer& integer(int64_t value);

    /**
     * Write a boolean value.
     */
    CborSerializer& boolean(bool trueOrFalse);

    /**
     * Write a single, definite text string.
     */
    CborSerializer& text(std::string_view value);

    /**
     * Write a single, definite binary string.
     */
    CborSerializer& binary(const BinaryBuffer& value);
    /**
     * Write an single, definite binary string. Its value is the concatenation of all given `buffers`.
     */
    CborSerializer& binary(StreamBuffers&& buffers);
    /**
     * Write a single, definite binary string.
     */
    CborSerializer& binary(std::string_view value);
    /**
     * Start an  indefinite binary string.
     * It must be followed by zero, one or more definite binary string items, followed
     * by `finishBinary()`.
     */
    CborSerializer& startBinary();
    CborSerializer& finishBinary();

    /**
     * Start an array of indefinite size.
     * You must call `finishArray()` when all array items have been written.
     */
    CborSerializer& startArray();
    /**
     * Start an array that contains `itemCount` items.
     * A call to `finishArray()` is not necessary.
     */
    CborSerializer& startArray(size_t itemCount);
    CborSerializer& finishArray();

    /**
     * Start a map of indefinite size.
     * You must call `finishMap()` when all key-value pairs have been written.
     * For each map item, first call `key()` and then write the value. The value can be single item,
     * like an integer, or can be a complex item, like another map.
     */
    CborSerializer& startMap();
    /**
     * Start a map that contains `pairCount` key-value pairs.
     * A call to `finishMap()` is not necessary.
     */
    CborSerializer& startMap(size_t pairCount);
    CborSerializer& finishMap();
    /**
     * Write a key (a definite text string). It must be followed by a single or a complex item.
     */
    CborSerializer& key(std::string_view value);

    /**
     * Write out any pending data to the underlying CborWriter and make sure that the
     * end of the stream flag is set.
     * This method is called from the destructor. Hence, you have to call it explicitly only
     * in cases where you want to access the content of the stream before the serializer is destroyed.
     *
     * Do not write more data after this method has been called.
     */
    void notifyEnd();

    template<CborSerializable T>
    static BinaryBuffer serialize(const T& t);

private:
    struct Context
    {
        enum Type
        {
            Plain,
            Binary,
            BinaryIndefinite,
            Text,
            TextIndefinite,
            Array,
            ArrayIndefinite,
            Map,
            MapIndefinite,
            Key
        };
        Type type;
        /**
         * Array:       number of missing items
         * Map:         number of missing pairs
         * Text,Binary: number of missing bytes
         */
        size_t missing;

        void decreaseMissing();
        void assertCanWriteBinary() const;
        bool appendingBinary() const;
        bool isArray() const;
        bool isMap() const;
    };

    CborWriter mWriter;
    Context mCurrentContext;
    std::stack<Context> mContextStack;
    bool mEos;

    void pushContextStack(Context context);
    void popContextStack();
};

namespace
{
    struct MemberSerializer
    {
        CborSerializer& serializer;

        /**
         * This function is one of the reasons why the `processMembers()` method of class T
         * that is being serialized must also be templated. It allows this implementation
         * to select the best fitting serialization code, depending on the member type.
         * This selection is done with `constexpr` `if` so that the compiler does most of the
         * work and the runtime code becomes a little faster.
         */
        template<typename T>
        void operator()(std::string_view path, const T& value)
        {
            // Remove a leading '/' from the path and expect the remainder to not contain another '/'.
            Assert(! path.empty());
            if (path[0] == '/')
                path = path.substr(1);
            Assert(path.find('/') == std::string_view::npos);

            // Write the key.
            serializer.key(std::string(path));

            // Write the value.
            // The list of recognized member (of T) types can be extended if the need arises.
            // NOLINTBEGIN
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, std::string>)
            {
                serializer.text(value);
            }
            else if constexpr (std::is_same_v<U, std::string_view>)
            {
                serializer.text(value);
            }
            else if constexpr (std::is_same_v<U, BinaryBuffer>)
            {
                serializer.binary(value);
            }
            else if constexpr (std::is_same_v<U, bool>)
            {
                serializer.boolean(value);
            }
            else if constexpr (std::is_same_v<U, Tee3Protocol::VauCid>)
            {
                serializer.text(value.value());
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_unsigned_v<T>)
                {
                    serializer.unsignedInteger(value);
                    return;
                }
                else
                {
                    if (value < 0)
                        serializer.negativeInteger(value);
                    else
                        serializer.unsignedInteger(static_cast<uint64_t>(value));
                }
            }
            else if constexpr (CborSerializable<U>)
            {
                // A complex type that is CborSerializable. This triggers a nested serialization
                // of type U.
                serializer.startMap();
                Cbor::applyProcessMembersToConstObject(value, *this);
                serializer.finishMap();
            }
            else if constexpr (std::is_same_v<U, std::vector<BinaryBuffer>>)
            {
                serializer.startArray(value.size());
                for (const auto& item : value)
                    serializer.binary(item);
                serializer.finishArray();
            }
            else if constexpr (std::is_enum_v<U>)
            {
                // Impose a limit on the range of enums. Has to be extended if the use case arises.
                serializer.unsignedInteger(static_cast<uint8_t>(value));
            }
            else
            {
                Failure() << "can not serialize type " << typeid(T).name() << " ("
                          << typeid(U).name() << ")";
            }
            // NOLINTEND
        }

        void operator()(std::string_view)
        {
            // This variant is primarily intended for deserialization so that the key and its value
            // are ignored. Therefore, they are ignored here as well.
        }
    };
}


template<CborSerializable T>
BinaryBuffer CborSerializer::serialize(const T& t)
{
    std::stringstream s;
    {
        auto stream = StreamFactory::makeWritableStream(s);
        CborSerializer serializer(stream);
        serializer.startMap();
        auto memberSerializer = MemberSerializer{.serializer = serializer};
        Cbor::applyProcessMembersToConstObject(t, memberSerializer);
        serializer.finishMap();
    } // The destructor of CborSerializer flushes pending bytes to `stream`.
    return BinaryBuffer(s.str());
}

} // namespace epa

#endif
