/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBOR_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBOR_HXX

#include "library/util/Assert.hxx"
#include "library/util/BinaryBuffer.hxx"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <string>
#include <tuple>
#include <variant>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

class ReadableByteStream;
class Stream;
class StreamBuffers;

namespace
{
    /**
     * A helper class to support the definition of the CborSerializable concept.
     */
    struct NoOpReadingProcessor
    {
        template<typename Value>
        void operator()(const std::string_view, const Value&)
        {
        }
    };
    /**
     * A helper class to support the definition of the CborDeserializable concept.
     */
    struct NoOpWritingProcessor
    {
        template<typename Value>
        void operator()(const std::string_view, Value&)
        {
        }
    };
    constexpr NoOpReadingProcessor noOpReadingProcessor;
    constexpr NoOpWritingProcessor noOpWritingProcessor;
}


/**
 * A concept that must be supported by a class T when T is to be serialized by the CborSerializer.
 * It is fulfilled when T has a method `processMembers()` that can be called with
 * a read-only processor instance. The `processMembers()` method defines key/value pairs
 * where the key is a path string that identifies a value in a CBOR serialization and value
 * is a reference to a C++ member of T.
 * Example:
 * struct Serializable
 * {
 *     int a;
 *     template<class Processor>
 *     void processMembers(Processor& p)
 *     {
 *         p("/a", "a");
 *     }
 * };
 */
// c
template<typename T>
concept CborSerializable = requires(T provider) { provider.processMembers(noOpReadingProcessor); };

/**
 * A concept that must be supported by a class T when T is to be deserialized by the CborDeserializer.
 * It is fulfilled when T has a method `processMembers()` that can be called with
 * a read-write processor instance. The `processMembers()` method defines key/value pairs
 * where the key is a path string that identifies a value in a CBOR serialization and value
 * is a reference to a C++ member of T.
 */
template<typename T>
concept CborDeserializable =
    requires(T provider) { provider.processMembers(noOpWritingProcessor); };


/**
 * The current CBOR support is focused on two use cases
 * - in the encryption of the export package
 * - serialization of data during the TEE handshake.
 *
 * Some features that are specified in RFC 8949 that are not supported are data types
 * floats, null, tags.
 * As a compensation the CborReader is able to stream large text or byte strings, regardless of how
 * they are encoded (definite or indefinite).
 */
class Cbor
{
public:
    /**
     * This `Type` enum is mostly an extension of `MajorType` with additional "secondary"
     * types added for major type 7 (aka break, aka simple, aka float).
     */
    enum class Type
    {
        // Taken from MajorType 0-6, 8
        UnsignedInteger,
        NegativeInteger,
        ByteString,
        TextString,
        Array,
        Map,
        Tag,
        EOS,

        // Break/Simple/Float types (for MajorType 7), according to RFC 8949, table 3.
        Simple,    // MajorType 7, any Info value not used below.
        True,      // MajorType 7, Info 20
        False,     // MajorType 7, Info 21
        Null,      // MajorType 7, Info 22
        HalfFloat, // MajorType 7, Info 24
        Float,     // MajorType 7, Info 25
        Double,    // MajorType 7, Info 26

        // The actual break (MajorType 7, Info 31)
        Break
    };

    /**
     * Major type values are located in the three top bits in the item head byte.
     */
    enum class MajorType : uint8_t
    {
        UnsignedInteger = 0,
        NegativeInteger = 1,
        ByteString = 2,
        TextString = 3,
        Array = 4,
        Map = 5,
        Tag = 6,
        BreakSimpleFloat = 7, // This is used also for floats and other special values. But
                              // in this implementation "break" is the most important one
                              // and hence defines the name.
        EOS = 8               // Synthetic type that signals the end of the stream.
    };

    /**
     * Return a text representation of the given `type`.
     */
    constexpr static std::string_view toString(const Cbor::Type type);

    /**
     * Return a text representation of the given major `type`.
     */
    constexpr static std::string_view toString(const Cbor::MajorType type);

    /**
     * Info values are located in the bottom five bits of the item head byte.
     */
    enum class Info : uint8_t
    {
        SmallestDirectValue = 0,

        // Values from 0 to 23, both included, can be encoded directly in the first byte of a
        // CBOR item.

        LargestDirectValue = 23,
        OneByteArgument = 24,
        TwoByteArgument = 25,
        FourByteArgument = 26,
        EightByteArgument = 27,

        // Values 28, 29 and 30 are reserved for future use.

        Indefinite = 31
    };

    /**
     * The special values for MajorType 7, i.e. break aka simple aka float, occupy the same space
     * as the Info values, i.e. the bottom 5 bits in the item head byte, but use different names.
     * I.e. use Simple when MajorType==7 and use Info when MajorType!=7
     */
    enum class Simple : uint8_t
    {
        SmallestEmbeddedSimple = 0,
        False = 20,
        True = 21,
        Null = 22,
        LargestEmbeddedSimple = 23,
        OneByteSimple = 24,
        HalfPrecisionFloat = 25,
        SinglePrecisionFLoat = 26,
        DoublePrecisionFloat = 27,
        // 28 -30 are reserved
        Break = 31
    };

    /**
     * Return a text representation of the given `info` value.
     * Note that all values for which there is no enum value, are converted to text representations
     * of their numerical values.
     */
    static std::string toString(const Cbor::Info info);

    /**
     * The CBOR item head defines type and size of an element
     * - the `type` is computed from both `majorType` and `info`
     * - the `majorType` is encoded in the top three bits of the first byte
     * - the `info` value that is encoded in the bottom five bits of the first byte
     *   note that `info` is called `simple` (or something similar) when `majorType` is
     *   7 (BreakSimpleFloat).
     * - the `length` is either identical to info, for numbers smaller than 24, or is
     *   encoded in the next 1, 2, 4, or 8 bytes.
     */
    struct ItemHead
    {
        const Type type;
        const MajorType majorType;
        const Info info;
        const uint64_t length;
    };
    static constexpr uint8_t CborMajorTypeMask = 0b1110'0000;
    static constexpr uint8_t CborInfoMask = 0b0001'1111;
    static constexpr size_t CborMajorTypeShift = 5;

    /**
     * Combine the two given values into a single byte.
     */
    static uint8_t makeItemHeadByte(MajorType type, Info info);
    /**
     * Extract the CBOR major type from the given byte.
     */
    static MajorType getMajorType(const uint8_t value);
    /**
     * Extract the CBOR type from the given byte.
     */
    static Type getType(const uint8_t value);
    /**
     * Extract the CBOR info value from the given byte.
     */
    static Info getInfo(const uint8_t value);

    /**
     * Extract the CBOR simple value from the given byte.
     * It has the same numerical value that `getInfo()` would return for the same byte
     * but uses different enum names.
     * Call getSimple() when majorType is 7. For all other values of majorType, call getInfo().
     */
    static Simple getSimple(const uint8_t value);

    /**
     * The Data class is used by the CborDeserializer. It is defined here primarily to have
     * a nicer name.
     */
    using Data = std::variant<
        uint8_t,
        uint16_t,
        uint32_t,
        uint64_t,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        StreamBuffers,
        std::vector<BinaryBuffer>,
        bool,
        std::monostate>;

    /**
     * Serialize a serializable object into a string. This is only useful in tests.
     */
    template<CborSerializable T>
    static std::string toString(const T& data);

    template<CborSerializable T, class Serializer>
    static void applyProcessMembersToConstObject(const T& object, Serializer& serializer);
};


constexpr std::string_view Cbor::toString(const Cbor::Type type)
{
#define Case(type)                                                                                 \
    case Cbor::Type::type:                                                                         \
        return #type
    switch (type)
    {
        Case(UnsignedInteger);
        Case(NegativeInteger);
        Case(ByteString);
        Case(TextString);
        Case(Array);
        Case(Map);
        Case(Tag);
        Case(Simple);
        Case(True);
        Case(False);
        Case(Null);
        Case(HalfFloat);
        Case(Float);
        Case(Double);
        Case(Break);
        Case(EOS);
    }
    Failure() << "invalid Cbor::MajorType value";
#undef Case
}


constexpr std::string_view Cbor::toString(const Cbor::MajorType type)
{
#define Case(type)                                                                                 \
    case Cbor::MajorType::type:                                                                    \
        return #type
    switch (type)
    {
        Case(UnsignedInteger);
        Case(NegativeInteger);
        Case(ByteString);
        Case(TextString);
        Case(Array);
        Case(Map);
        Case(Tag);
        Case(BreakSimpleFloat);
        Case(EOS);
    }
    Failure() << "invalid Cbor::MajorType value";
#undef Case
}


namespace
{
    template<typename T>
    concept HasStreamOperator = requires(T a) { std::stringstream() << a; };

    /**
     * This implementation of the CborSerializable concept helps to print the key/value
     * pairs of serializable objects.
     */
    struct ToStringSerializer
    {
        struct Row
        {
            const std::string path;
            const std::string value;
        };
        std::vector<Row> rows;
        std::string pathPrefix;
        size_t maxPathLength = 0;

        /**
         * Collect key/value pairs but do not yet print them.
         * This allows us to determine the longest key.
         */
        template<typename T>
        void operator()(const std::string_view path, const T& data)
        {
            // Serialize the `data` value. The special handling for BinaryBuffer could be
            // moved to an operator<<() implementation for that type.
            std::stringstream value;
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_same_v<U, BinaryBuffer>)
                value << "BinaryBuffer of length " << data.size();
            else if constexpr (
                std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>)
                value << '"' << data << '"';
            else if constexpr (std::is_same_v<U, bool>)
                value << (data ? "True" : "False");
            else if constexpr (HasStreamOperator<T>)
                value << data;
            else
            {
                // Recursively serialize data as nested map under the assumption that T supports
                // this.
                const auto savedPathPrefix = pathPrefix;
                pathPrefix += std::string(path);
                Cbor::applyProcessMembersToConstObject(data, *this);
                pathPrefix = savedPathPrefix;

                // Do not add an extra row for `path` itself.
                return;
            }

            // Remember the longest path length, so that the values can be printed at the same column.
            // NOLINTNEXTLINE(readability-use-std-min-max)
            if (maxPathLength < path.size())
                maxPathLength = path.size();

            rows.emplace_back(Row{.path = pathPrefix + std::string(path), .value = value.str()});
        }

        /**
         * Print all registered key/value pairs. Values are aligned for better readability.
         */
        std::string format()
        {
            std::stringstream s;
            bool first = true;
            for (const auto& row : rows)
            {
                if (first)
                    first = false;
                else
                    s << '\n';
                s << std::setw(gsl::narrow_cast<int>(maxPathLength)) << std::left << row.path
                  << " : " << row.value;
            }
            return s.str();
        }
    };
}


template<CborSerializable T>
std::string Cbor::toString(const T& data)
{
    ToStringSerializer printer;
    applyProcessMembersToConstObject(data, printer);
    return printer.format();
}


template<CborSerializable T, class Serializer>
void Cbor::applyProcessMembersToConstObject(const T& object, Serializer& serializer)
{
    // Cast away the const-ness of `object` so that we can call its `processMembers()` method
    // but we will still only read from `object`, not make any modifications.
    // This is the price to pay for not requiring a duplicate `processMembers()` method that
    // differs only in its const-ness.
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    const_cast<T&>(object).processMembers(serializer);
}

} // namespace epa

#endif
