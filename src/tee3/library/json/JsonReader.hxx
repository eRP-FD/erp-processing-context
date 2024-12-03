/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_JSON_JSONREADER_HXX
#define EPA_LIBRARY_JSON_JSONREADER_HXX

// Needs to be defined when including rapidjson/fwd.h so that there are no conflict when the real
// rapidjson headers are included.
#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif

#include "library/json/BinaryDocument.hxx"

#include <boost/core/noncopyable.hpp>
#include <rapidjson/fwd.h>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

/**
 * A JSON value could not be read. For compatibility with earlier versions of our JSON code, the
 * base class is std::invalid_argument, which is what the earlier JsonMapper class used.
 *
 * This exception is only used by JsonReader, not JsonWriter, because it indicates invalid JSON
 * input, which can only happen when reading. Any error in JsonWriter is an application logic bug.
 */
class JsonError : public std::invalid_argument
{
public:
    explicit JsonError(const std::string& what)
      : invalid_argument{what}
    {
    }
};


/**
 * JsonReader parses a single JSON value from a string. It can either be a JSON object, in which
 * case any of the `read...(key)` methods can be used to retrieve values by key, or it can be an
 * individual value, in which case the `...Value()` methods can be used to parse it into a
 * value of type T. JsonReader supports strings, integers, floating-point values, and booleans out
 * of the box, and all custom types that implement a static fromJson method.
 */
class JsonReader : private boost::noncopyable
{
public:
    /** Parses the given JSON string, or throws JsonError. */
    explicit JsonReader(std::string_view json);

    ~JsonReader() noexcept;

    /**
     * Validates this JSON document against the given schema.
     *
     * @throw JsonError if the document does not match the schema.
     */
    void validateSchema(std::string_view schema) const;

    /** Returns the JSONPath expression (dot notation) that leads to the current node. */
    std::string getPath() const;

    /** Converts the JSON value represented by this JsonReader to a string. */
    std::string toJsonString() const;

    /** Returns the node value if it is a string, or throws JsonError. */
    std::string stringValue() const;

    std::string_view view() const;

    /** Returns the node value if it is an int64_t, or throws JsonError. */
    std::int64_t int64Value() const;
    /** Returns the node value if it is a uint64_t, or throws JsonError. */
    std::uint64_t uint64Value() const;
    /** Returns the node value if it is a double, or throws JsonError. */
    double doubleValue() const;
    /** Returns the node value if it is a boolean value, or throws JsonError. */
    bool boolValue() const;

    /**
     * Returns true if this JSON object has a non-null member with the given name.
     *
     * @throw JsonError if this is not a JSON object.
     */
    bool hasMember(std::string_view key) const;

    /**
     * Returns true if this JsonReader refers to a single string value. This is not generally
     * useful, but might be necessary in case the type of a value changes over time.
     * (Example: See AvailabilityStatus::fromJson.)
     */
    bool isStringValue() const;

    /**
     * Returns true if this JsonReader refers to an array of a any value type.
     */
    bool isArrayValue() const;

    /** Returns the elements of an array node, or throws JsonError. */
    template<typename T, template<typename, typename...> class C = std::vector>
    C<T> arrayValue() const
    {
        const std::size_t size = arraySize();

        C<T> container;
        if constexpr (std::is_same_v<C<T>, std::vector<T>>)
            container.reserve(size);

        for (std::size_t index = 0; index < size; ++index)
        {
            const JsonReader arrayValue{*this, index};
            container.insert(container.end(), fromJson<T>(arrayValue));
        }
        return container;
    }

    /** Reads the value of the given key in this JSON object, or throws JsonError. */
    template<typename T>
    T read(std::string_view key) const
    {
        const JsonReader nestedReader{*this, key};
        return fromJson<T>(nestedReader);
    }

    /** Like read(), but returns nullopt for missing or null values. */
    template<typename T>
    std::optional<T> readOptional(std::string_view key) const
    {
        return hasMember(key) ? std::make_optional(read<T>(key)) : std::nullopt;
    }

    template<typename T, template<typename, typename...> class C = std::vector>
    C<T> readArray(std::string_view key) const
    {
        const JsonReader nestedReader{*this, key};
        return nestedReader.arrayValue<T, C>();
    }

    template<typename T, template<typename, typename...> class C = std::vector>
    std::optional<C<T>> readOptionalArray(std::string_view key) const
    {
        return hasMember(key) ? std::make_optional(readArray<T, C>(key)) : std::nullopt;
    }

    template<typename T, template<typename, typename...> class C = std::vector>
    C<T> readArrayOrEmpty(std::string_view key) const
    {
        return hasMember(key) ? readArray<T, C>(key) : C<T>{};
    }

    template<typename T, template<typename, typename...> class C = std::vector>
    C<C<T>> readArrayOfArrays(std::string_view key) const
    {
        const JsonReader outerArrayReader{*this, key};
        const std::size_t outerSize = outerArrayReader.arraySize();

        C<C<T>> outerContainer;
        if constexpr (std::is_same_v<C<T>, std::vector<T>>)
            outerContainer.reserve(outerSize);

        for (std::size_t outerIndex = 0; outerIndex < outerSize; ++outerIndex)
        {
            const JsonReader innerArrayReader{outerArrayReader, outerIndex};
            outerContainer.insert(outerContainer.end(), innerArrayReader.arrayValue<T, C>());
        }
        return outerContainer;
    }

    template<typename T, template<typename, typename...> class C = std::vector>
    std::optional<C<C<T>>> readOptionalArrayOfArrays(std::string_view key) const
    {
        return hasMember(key) ? std::make_optional(readArrayOfArrays<T, C>(key)) : std::nullopt;
    }

    /** Returns all the keys in this object. This includes keys for which the value is null. */
    template<typename T = std::string, template<typename, typename...> class C = std::vector>
    C<T> getMapKeys() const
    {
        const std::size_t numberOfKeys = objectSize();
        C<T> keys;
        if constexpr (std::is_same_v<C<T>, std::vector<T>>)
            keys.reserve(numberOfKeys);
        for (std::size_t index = 0; index < numberOfKeys; ++index)
        {
            const JsonReader keyReader{*this, index};
            keys.insert(keys.end(), fromJson<T>(keyReader));
        }
        return keys;
    }

    /**
     * Reads a JSON object into a key-value map.
     *
     * If a key appears multiple times in JSON, the first occurrence wins. This is for compatibility
     * with the old JsonMapper class, but should also never happen. If you think it might happen,
     * you can pass a multimap as the template template parameter.
     */
    template<typename K, typename V, template<typename, typename, typename...> class M = std::map>
    M<K, V> readMap(std::string_view key) const
    {
        const JsonReader nestedReader{*this, key};
        const std::size_t size = nestedReader.objectSize();

        M<K, V> map;
        for (std::size_t index = 0; index < size; ++index)
        {
            const JsonReader keyReader{nestedReader, index};
            const JsonReader valueReader{nestedReader, keyReader.stringValue()};
            map.emplace(fromJson<K>(keyReader), fromJson<V>(valueReader));
        }
        return map;
    }

    /**
     * Reads all arithmetic types, strings, and types that have a static fromJson method.
     * It is intended to be specialized for types where a static fromJson method is not a good fit.
     */
    template<typename T>
    static T fromJson(const JsonReader& reader)
    {
        // Prevent accidental std::string arguments in JsonReader::readArray.
        static_assert(! std::is_same_v<T, char>, "JsonReader does not support reading 'char'");

        if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
            return reader.stringValue();
        else if constexpr (std::is_same_v<T, bool>)
            return reader.boolValue();
        else if constexpr (std::is_floating_point_v<T>)
            return reader.doubleValue();
        else if constexpr (std::is_unsigned_v<T>)
            try
            {
                return gsl::narrow<T>(reader.uint64Value());
            }
            catch (const gsl::narrowing_error&)
            {
                throw JsonError{"Integer value out of range at path: " + reader.getPath()};
            }
        else if constexpr (std::is_signed_v<T>)
            try
            {
                return gsl::narrow<T>(reader.int64Value());
            }
            catch (const gsl::narrowing_error&)
            {
                throw JsonError{"Integer value out of range at path: " + reader.getPath()};
            }
        else
            return T::fromJson(reader);
    }

    /**
     * Convenience method to deserialize a JSON string into any kind of object that has JSON
     * support. Counterpart to JsonWriter::toClientJson and JsonWriter::toBackendJson.
     */
    template<typename T>
    static T parse(std::string_view json)
    {
        const JsonReader reader{json};
        return fromJson<T>(reader);
    }

private:
    /** The full rapidjson::Document. Only the root JsonReader has this. */
    std::unique_ptr<rapidjson::Document> mDocument;
    std::unique_ptr<BinaryDocument> mBinaryDocument;
    /** The rapidjson::Value that is referenced by this node. */
    const rapidjson::Value* mValue = nullptr;
    const BinaryDocument::Value* mBinValue = nullptr;
    /** The parent JsonReader, for constructing getPath(). */
    const JsonReader* const mParent = nullptr;
    /** The key of this JsonReader, for constructing getPath(). */
    const std::string_view mKey{};
    /** The index of this JsonReader, for constructing getPath(). */
    const std::size_t mIndex{};

    /** Creates a nested JsonReader for the value associated with `key` in `parent`. */
    JsonReader(const JsonReader& parent, std::string_view key);
    /** Creates a nested JsonReader for the index-th object key or array element in `parent`. */
    JsonReader(const JsonReader& parent, std::size_t index);

    /** Returns the length of this array, or throws JsonError. */
    std::size_t arraySize() const;
    /** Returns the length (number of keys) in this object, or throws JsonError. */
    std::size_t objectSize() const;

#ifdef FRIEND_TEST
    friend class WellKnownOpenidFederationTest;
    FRIEND_TEST(WellKnownOpenidFederationTest, getWellKnownOpenidFederation);
#endif
};

} // namespace epa
#endif
