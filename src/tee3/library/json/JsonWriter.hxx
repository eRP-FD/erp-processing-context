/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_JSON_JSONWRITER_HXX
#define EPA_LIBRARY_JSON_JSONWRITER_HXX

//##############################################################################
//################## Configuration of testing binary encoding ##################
//##############################################################################

// Uncomment the following line if you want JsonWriter::toBackendBinary(..) to really return binary
// documents instead of JSON (the JsonReader always accepts both).
//#define ENABLE_BACKEND_BINARY

// Uncomment the following line if you want JsonWriter::toBackendBinary(..) to test whether writing
// and reading the binary document results identical to the original data. This is expensive, please
// disable when benchmarking.
//#define TEST_BACKEND_BINARY

// Uncomment the following line if you want JsonWriter::toBackendBinary(..) to collect and print
// statistic data about the size of JSON and binary, compressed and not compressed. This is
// expensive, please disable when benchmarking.
//#define STAT_BACKEND_BINARY

//##############################################################################

#if defined(TEST_BACKEND_BINARY)
#include "library/json/JsonReader.hxx"
#endif

#if defined(STAT_BACKEND_BINARY)
#include "library/log/Logging.hxx"
#include "library/util/Compression.hxx"

#include <boost/core/demangle.hpp>
#include <iostream>
namespace epa
{
class Event;
}
#endif

#include "library/util/Time.hxx"

#include <boost/core/noncopyable.hpp>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
namespace epa
{

class BinaryDocument;


/**
 * Some classes want to write extra fields depending on how they are being used, e.g. they might
 * include redundant information that is used by the client (FdV) SDK but should not be written when
 * the JSON is stored in a database for long-term storage.
 */
enum class JsonVariant
{
    /** For communication between C++ and other languages. Never persisted. */
    CLIENT,
    /** For long-term database storage. This variant must be stable and backwards-compatible. */
    BACKEND_PERSISTENCE,
    /** For communication with JSON-based APIs. */
    NETWORK
};


/**
 * Represents a single JSON "value", that is, either a scalar value (integer, string, boolean),
 * a key-value object, or an array.
 *
 * Types that support JSON serialization must either have a `writeJson(JsonWriter&) const` method,
 * or if that is not possible, an overload of the global `writeJson(JsonWriter&, T)` function.
 */
class JsonWriter : private boost::noncopyable
{
public:
    explicit JsonWriter(JsonVariant variant, bool binary = false);
    ~JsonWriter();

    JsonVariant getVariant() const;

    /**
     * Returns the JSON that was written up to this point as a string.
     *
     * Because this method might have to append the last trailing '}' character, it is not `const`,
     * and writing more content into a JsonWriter after `toString` has been called is an error.
     */
    std::string toString();

    /**
     * Turns this writer into a string value.
     *
     * If you want other string-like types (enums, UUID etc.) to be serialized as a string, add a
     * writeJson() method to them that calls this method, or overload the global writeJson() method
     * for this type.
     */
    void makeStringValue(std::string_view value);
    /** Used to implement writeJson(JsonWriter&, <arithmetic_type>). */
    void makeInt64Value(std::int64_t value);
    /** Used to implement writeJson(JsonWriter&, <arithmetic_type>). */
    void makeUint64Value(std::uint64_t value);
    /** Used to implement writeJson(JsonWriter&, <arithmetic_type>). */
    void makeDoubleValue(double value);
    /** Used to implement writeJson(JsonWriter&, <arithmetic_type>). */
    void makeBoolValue(bool value);

    /** Writes the given raw JSON. Passing invalid JSON will result in broken output. */
    void makeJsonObject(std::string_view json);

    /**
     * Writes an array of the given size.
     *
     * @param callback A functor that will be called with the index and JsonWriter of the given
     * value inside the array.
     */
    void makeArrayValue(
        std::size_t size,
        const std::function<void(std::size_t, JsonWriter&)>& callback);
    /** Convenience overload that writes a homogeneous container. */
    template<typename C>
    void makeArrayValue(const C& values)
    {
        auto iterator = values.begin();
        makeArrayValue(values.size(), [&iterator](std::size_t, JsonWriter& writer) {
            writeJson(writer, *iterator);
            iterator++;
        });
    }

    /**
     * Writes a key-value pair. Turns this writer into a JSON object, if it is not yet one.
     *
     * The value will be written using writeJson(JsonWriter&, T).
     */
    template<typename T>
    void write(std::string_view key, const T& value)
    {
        JsonWriter keyWriter{*this, OBJECT_KEY};
        keyWriter.makeStringValue(key);
        keyWriter.finalize();
        JsonWriter valueWriter{*this, UNDECIDED};
        writeJson(valueWriter, value);
    }

    template<typename T>
    void writeOptional(std::string_view key, const std::optional<T>& value)
    {
        if (value.has_value())
            write(key, *value);
    }

    template<typename C>
    void writeArray(std::string_view key, const C& values)
    {
        JsonWriter keyWriter{*this, OBJECT_KEY};
        keyWriter.makeStringValue(key);
        keyWriter.finalize();
        JsonWriter arrayWriter{*this, UNDECIDED};
        arrayWriter.makeArrayValue(values);
    }

    template<typename C>
    void writeOptionalArray(std::string_view key, const std::optional<C>& values)
    {
        if (values.has_value())
            writeArray(key, *values);
    }

    template<typename C>
    void writeArrayUnlessEmpty(std::string_view key, const C& values)
    {
        if (! values.empty())
            writeArray(key, values);
    }

    template<typename CC>
    void writeArrayOfArrays(std::string_view key, const CC& nestedValues)
    {
        JsonWriter keyWriter{*this, OBJECT_KEY};
        keyWriter.makeStringValue(key);
        keyWriter.finalize();
        JsonWriter outerArrayWriter{*this, ARRAY};
        for (const auto& values : nestedValues)
        {
            JsonWriter innerArrayWriter{outerArrayWriter, UNDECIDED};
            innerArrayWriter.makeArrayValue(values);
        }
    }

    template<typename CC>
    void writeOptionalArrayOfArrays(std::string_view key, const std::optional<CC>& nestedValues)
    {
        if (nestedValues.has_value())
            writeArrayOfArrays(key, *nestedValues);
    }

    template<typename M>
    void writeMap(std::string_view key, const M& map)
    {
        JsonWriter outerKeyWriter{*this, OBJECT_KEY};
        outerKeyWriter.makeStringValue(key);
        outerKeyWriter.finalize();
        JsonWriter mapWriter{*this, OBJECT};
        for (const auto& [nestedKey, value] : map)
        {
            static_assert(
                ! std::is_arithmetic_v<decltype(nestedKey)>, "JSON does not support numeric keys");

            JsonWriter innerKeyWriter{mapWriter, OBJECT_KEY};
            writeJson(innerKeyWriter, nestedKey);
            innerKeyWriter.finalize();
            JsonWriter mapValue{mapWriter, UNDECIDED};
            writeJson(mapValue, value);
        }
    }

    /**
     * Convenience function that converts an arbitrary value (that supports writeJson) to a JSON
     * string, using JsonVariant::CLIENT.
     *
     * @note This is often different from calling enum.toJsonString() for types that still have
     * such a method, because JsonWriter::toString(enum) will return "\\"enum\\"".
     */
    template<typename T>
    static std::string toClientJson(const T& value)
    {
        JsonWriter writer{JsonVariant::CLIENT};
        writeJson(writer, value);
        return writer.toString();
    }

    /**
     * Convenience function that converts an arbitrary value (that supports writeJson) to a JSON
     * string, using JsonVariant::NETWORK.
     */
    template<typename T>
    static std::string toNetworkJson(const T& value)
    {
        JsonWriter writer{JsonVariant::NETWORK};
        writeJson(writer, value);
        return writer.toString();
    }

    /**
     * Convenience function that converts an arbitrary value (that supports writeJson) to a JSON
     * string, using JsonVariant::BACKEND_PERSISTENCE.
     *
     * @note This is often different from calling enum.toJsonString() for types that still have
     * such a method, because JsonWriter::toString(enum) will return "\\"enum\\"".
     */
    template<typename T>
    static std::string toBackendJson(const T& value)
    {
        JsonWriter writer{JsonVariant::BACKEND_PERSISTENCE};
        writeJson(writer, value);
        return writer.toString();
    }

    template<typename T>
    static std::string toBackendBinary(const T& value)
    {
#if defined(STAT_BACKEND_BINARY)
        static std::chrono::high_resolution_clock::duration totalEncodeTime = {};
        static std::chrono::high_resolution_clock::duration totalDecodeTime = {};
#endif

#if ! defined(ENABLE_BACKEND_BINARY) || defined(TEST_BACKEND_BINARY) || defined(STAT_BACKEND_BINARY)
        std::string jsonDoc = toBackendJson(value);
#endif

#if defined(ENABLE_BACKEND_BINARY) || defined(TEST_BACKEND_BINARY) || defined(STAT_BACKEND_BINARY)
        std::string binaryDoc;
        {
#if defined(STAT_BACKEND_BINARY)
            auto startTime = std::chrono::high_resolution_clock::now();
#endif
            JsonWriter writer{JsonVariant::BACKEND_PERSISTENCE, true};
            writeJson(writer, value);
            binaryDoc = writer.toString();
#if defined(STAT_BACKEND_BINARY)
            totalEncodeTime += std::chrono::high_resolution_clock::now() - startTime;
#endif
        }
#endif

#if defined(TEST_BACKEND_BINARY) || defined(STAT_BACKEND_BINARY)

        {
#if defined(STAT_BACKEND_BINARY)
            auto startTime = std::chrono::high_resolution_clock::now();
#endif
            T valueAfter = JsonReader::fromJson<T>(JsonReader(binaryDoc));
#if defined(STAT_BACKEND_BINARY)
            totalDecodeTime += std::chrono::high_resolution_clock::now() - startTime;
#endif
#if defined(TEST_BACKEND_BINARY)
            std::string jsonDocAfter = toBackendJson(valueAfter);
            if (jsonDoc != jsonDocAfter)
            {
                throw std::runtime_error(
                    "Binary self-test failed. JSON before: " + jsonDoc
                    + "\n JSON after binary write/read: " + jsonDocAfter);
            }
#endif
        }
#endif

#if defined(STAT_BACKEND_BINARY)
        {
            constexpr Compression::TextTypeHint typeHint =
                std::is_same_v<T, Event> ? Compression::TextTypeHint::AuditLog
                                         : Compression::TextTypeHint::Metadata;

            auto compressedJsonDoc = Compression::compress(jsonDoc, typeHint);
            auto compressedBinaryDoc = Compression::compress(binaryDoc, typeHint);

            static const auto type = boost::core::demangle(typeid(T).name());

            //LOG(INFO) << "JSON dump of " << type << ": " << jsonDoc;
            //LOG(INFO) << "Binary dump of " << type << ": " << binaryDoc;

            LOG(INFO) << "Size of a single " << type << ": JSON = " << jsonDoc.size()
                      << ", binary = " << binaryDoc.size()
                      << ": compressed JSON = " << compressedJsonDoc.size()
                      << ", compressed binary = " << compressedBinaryDoc.size();

            static size_t totalNumber = 0;
            static size_t totalJsonSize = 0;
            static size_t totalBinarySize = 0;
            static size_t totalCompressedJsonSize = 0;
            static size_t totalCompressedBinarySize = 0;

            totalNumber++;
            totalJsonSize += jsonDoc.size();
            totalBinarySize += binaryDoc.size();
            totalCompressedJsonSize += compressedJsonDoc.size();
            totalCompressedBinarySize += compressedBinaryDoc.size();

            static const struct OnProgramExit
            {
                ~OnProgramExit()
                {
                    // Using std::cout because logging could have already been destructed - right?
                    std::cout
                        << "Total size of all " << totalNumber << " processed " << type
                        << "s: JSON = " << totalJsonSize << ", binary = " << totalBinarySize
                        << ", compressed JSON = " << totalCompressedJsonSize
                        << ", compressed binary = " << totalCompressedBinarySize
                        << "; binary encoding time = "
                        << std::chrono::duration_cast<std::chrono::nanoseconds>(totalEncodeTime)
                                   .count()
                               / 1000000.0
                        << " ms, binary decoding time = "
                        << std::chrono::duration_cast<std::chrono::nanoseconds>(totalDecodeTime)
                                   .count()
                               / 1000000.0
                        << " ms" << std::endl
                        << "    => compressed binary is "
                        << (1000 - totalCompressedBinarySize * 1000 / totalCompressedJsonSize)
                               / 10.0
                        << " percent smaller than compressed JSON" << std::endl;
                }
            } onProgramExit;
        }
#endif

#if defined(ENABLE_BACKEND_BINARY)
        return binaryDoc;
#else
        return jsonDoc;
#endif
    }

private:
    const JsonVariant mVariant;

    struct RapidJsonWriter;
    std::unique_ptr<RapidJsonWriter> mWriter;
    std::unique_ptr<BinaryDocument> mBinDoc;

    JsonWriter* const mParentWriter = nullptr;

    enum DataType
    {
        /** The type is initially undecided. If nothing is written, default to an empty object. */
        UNDECIDED,
        /** Either a string, integer, floating-point, or boolean value. */
        SINGLE_VALUE,
        /**
         * JSON keys are special because they need to be written using RapidJSON's Key() method, not
         * the String() method as for string values. This data type indicates that this JsonWriter
         * expects a string value. (After one is written, the data type is set to SINGLE_VALUE to
         * avoid further writes.)
         *
         * This mechanism allows JsonWriter::writeMap to support any type as the key which has a
         * writeJson method or function overload.
         */
        OBJECT_KEY,
        OBJECT,
        ARRAY,
        /** This value has written all closing braces (if any) and is now read-only. */
        FINALIZED_JSON
    };
    DataType mDataType;

    /** Starts writing a nested value. */
    JsonWriter(JsonWriter& parent, DataType dataType);
    /** Closes the braces opened by this level, if any, or writes the default value: {}. */
    void finalize() noexcept;
};


/** Writes an arbitrary string view to the given writer. */
inline void writeJson(JsonWriter& writer, std::string_view value)
{
    writer.makeStringValue(value);
}


/**
 * This overload handles all arithmetic types. This is done to avoid the messiness of automatic
 * conversions, especially to bool. E.g., writeJson(w, "string") might otherwise prefer to convert
 * its second argument to bool instead of std::string_view.
 */
template<typename T>
auto writeJson(JsonWriter& writer, T value)
    requires std::is_arithmetic_v<T>
{
    // Prevent accidental std::string arguments in JsonWriter::writeArray.
    static_assert(! std::is_same_v<T, char>, "JsonWriter does not support writing 'char'");

    if constexpr (std::is_same_v<T, bool>)
        writer.makeBoolValue(value);
    else if constexpr (std::is_floating_point_v<T>)
        writer.makeDoubleValue(value);
    else if constexpr (std::is_unsigned_v<T>)
        writer.makeUint64Value(value);
    else
        writer.makeInt64Value(value);
}


template<typename T>
void writeJson(JsonWriter& writer, const T& value)
    requires requires {
        { value.writeJson(writer) } -> std::same_as<void>;
    }
{
    value.writeJson(writer);
}


template<typename... Ts>
void writeJson(JsonWriter& writer, const std::variant<Ts...>& variant)
{
    std::visit(
        [&writer]<typename T>(T&& arg) { return writeJson(writer, std::forward<T>(arg)); },
        variant);
}

} // namespace epa

#endif
