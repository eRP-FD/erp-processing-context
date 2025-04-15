/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_ENUMTABLE_HXX
#define EPA_LIBRARY_UTIL_ENUMTABLE_HXX

#include "library/util/EnumMacros.hxx"

#include <boost/core/noncopyable.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits> // for std::is_enum_v
#include <typeinfo>
#include <unordered_map>
#include <utility> // for std::move
#include <vector>

namespace epa
{

/** A templated exception class for table lookup errors. */
template<typename EnumOrWrapper>
class UnsupportedEnumValue : public std::runtime_error
{
public:
    explicit UnsupportedEnumValue(const std::string& text)
      : std::runtime_error{text}
    {
    }
};


/** Internal helper function for the primary EnumTable constructor. Global to avoid templating. */
std::vector<std::string> splitRawEnumNames(std::string_view rawNames);


/**
 * This class parses and stores the output of the EPA_VALUES_ENUM and EPA_ENUM_CLASS macros.
 * It enables conversion between enums and strings without writing any additional code.
 *
 * @tparam EnumOrWrapper An enum type, or the wrapper class around an enum type (e.g. AccessLevel).
 * @tparam StringType This is usually std::string, but sometimes we want to map from/to other types
 *                    as well. In this case, the "base table" can be converted by using the
 *                    EnumTable::transformed method.
 *                    There must be an operator<< to print StringType to an std::ostream for the
 *                    purpose of building exception strings.
 */
template<typename EnumOrWrapper, typename StringType = std::string>
class EnumTable : boost::noncopyable
{
public:
    /**
     * @param rawNames A stringified definition of enum values, such as "FOO,\n  BAR,\n  QUX",
     *                 which would be parsed into the vector {"FOO", "BAR", "QUX"}.
     */
    explicit EnumTable(std::string_view rawNames)
      : EnumTable{splitRawEnumNames(rawNames)}
    {
    }

    /** Direct construction from given names, mostly intended to implement transformed. */
    explicit EnumTable(std::vector<StringType> names)
      : mNames{std::move(names)},
        mLookupTable{buildLookupTable()}
    {
    }

    /**
     * If an enum wrapper has more than one conversion from/to string, it can call this method to
     * create a derived EnumTable by providing a functor that maps from EnumToWrapper to another
     * type. The returned map can then be used to convert from/to the new value.
     *
     * By implementing the functor based on a switch() statement, you can let the compiler check
     * that no value has been forgotten.
     */
    template<typename NewStringType = std::string, typename Functor>
    EnumTable<EnumOrWrapper, NewStringType> transformed(const Functor& functor) const
    {
        std::vector<NewStringType> transformedNames;
        transformedNames.reserve(mNames.size());
        for (std::size_t index = 0; index < mNames.size(); ++index)
        {
            const EnumOrWrapper enumValue = fromIndex(index);
            transformedNames.push_back(functor(enumValue));
        }
        return EnumTable<EnumOrWrapper, NewStringType>{std::move(transformedNames)};
    }

    /**
     * Converts a given enum value into its string representation.
     *
     * @tparam Exception The exception to throw if the value is out of range.
     *                   When implementing the toJsonString method, please use JsonError.
     */
    template<typename Exception = UnsupportedEnumValue<EnumOrWrapper>>
    const StringType& toString(EnumOrWrapper value) const
    {
        const std::size_t index = toIndex(value);
        if (index >= mNames.size())
        {
            const std::string enumName = typeid(EnumOrWrapper).name();
            throw Exception{enumName + " out of range: " + std::to_string(index)};
        }
        return mNames[index];
    }

    /**
     * Looks up an enum value by its string representation.
     *
     * @tparam Exception The exception to throw if the string value cannot be found.
     *                   When implementing the fromJsonString method, please use JsonError.
     */
    template<typename Exception = UnsupportedEnumValue<EnumOrWrapper>>
    EnumOrWrapper fromString(const StringType& name) const
    {
        const auto it = mLookupTable.find(name);
        if (it == mLookupTable.end())
        {
            std::ostringstream errorMessage;
            errorMessage << typeid(EnumOrWrapper).name() << " does not include " << name;
            throw Exception{errorMessage.str()};
        }

        return fromIndex(it->second);
    }

    std::vector<EnumOrWrapper> getAllValues() const
    {
        std::vector<EnumOrWrapper> result;
        result.reserve(mNames.size());
        for (std::size_t index = 0; index < mNames.size(); ++index)
            result.push_back(fromIndex(index));
        return result;
    }

private:
    /** For efficient mapping from index to name. */
    const std::vector<StringType> mNames;
    /** For efficient mapping from name to index. */
    const std::unordered_map<StringType, std::size_t> mLookupTable;

    /** Reverse mapping, not part of the constructor so that mLookupTable can be const. */
    std::unordered_map<StringType, std::size_t> buildLookupTable() const
    {
        std::unordered_map<StringType, std::size_t> lookupTable;
        for (std::size_t index = 0; index < mNames.size(); ++index)
        {
            const auto [iterator, didInsert] = lookupTable.emplace(mNames[index], index);
            (void) iterator;
            if (! didInsert)
            {
                std::ostringstream errorMessage;
                const char* enumName = typeid(EnumOrWrapper).name();
                errorMessage << mNames[index] << " appears twice in EnumTable for " << enumName;
                throw std::logic_error{errorMessage.str()};
            }
        }
        return lookupTable;
    }

    /** Internal helper for index->enum conversions that handles enum wrapper classes. */
    static EnumOrWrapper fromIndex(std::size_t index)
    {
        if constexpr (std::is_enum_v<EnumOrWrapper>)
            return static_cast<EnumOrWrapper>(index);
        else
            return EnumOrWrapper{static_cast<typename EnumOrWrapper::Value>(index)};
    }

    /** Internal helper for enum->index conversions that handles enum wrapper classes. */
    static std::size_t toIndex(EnumOrWrapper value)
    {
        if constexpr (std::is_enum_v<EnumOrWrapper>)
            return static_cast<std::size_t>(value);
        else
            return static_cast<std::size_t>(value.getValue());
    }
};

} // namespace epa

#endif
