/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_NUMBERASSTRINGPARSERDOCUMENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_NUMBERASSTRINGPARSERDOCUMENT_HXX

#ifdef RAPIDJSON_ASSERT
#undef RAPIDJSON_ASSERT
#endif
#include "erp/util/Expect.hxx"
#define RAPIDJSON_ASSERT(x) ModelExpect(x, "rapidjson assertion")

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>

#include <optional>
#include <string_view>

namespace model
{
class ResourceBase;
template<class Stream> class NumberAsStringParserWriter;

/**
 * The FHIR specification requires number precisions in JSON files to be reproduced with the
 * original trailing zeros.
 *
 * To parse a json string into a json document for maintaining number precisions and inserting
 * the prefixes you must use the "NumberAsStringParserDocument::ParseStream" method as follows:
 *
 *     NumberAsStringParserDocument document;
 *     rapidjson::StringStream s(jsonString.data());
 *     document.ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s);
 *
 * To create a json string from the document removing the prefixes you have to use the
 * "NumberAsStringParserWriter" as follows:
 *
 *     auto document = model.jsonDocument();
 *     rapidjson::StringBuffer buffer;
 *     NumberAsStringParserWriter<rapidjson::StringBuffer> writer(buffer);
 *     document.Accept(writer);
 *
 * More details:
 * -------------
 *
 * A value like 1.10 would be read then it has to be written with the trailing zero.
 * To achieve this we have to modify our usage of rapidjson because by default trailing zeros
 * (after the decimal point) as well as leading zeros are removed.
 *
 * For this when parsing the json string the "kParseNumbersAsStringsFlag" is used together
 * with a custom handler that overrides "RawNumber" and "String" prefixing raw number strings
 * with '#' and regular strings with '$'. The custom reader handler is provided by class
 * "model::NumberAsStringParserDocument".
 *
 * For writing JSON also a custom handler is used that makes the inverse conversion and
 * removes the leading '#' or '$' from strings and outputs the rest of the string without
 * or with enclosing double quotes. The custom writer handler is provided by class
 * "model::NumberAsStringParserWriter".
 *
 * The values of the rapidjson document used by the resource model contain the prefixes
 * for string and number values. This has to be taken into account when modifying or reading
 * the values from the document during runtime.
 *
 * Some setter methods like "setKeyValue" or "setValue" automatically insert the prefixes for
 * strings and number value types.
 *
 * Some getter methods like "getStringValue" or "getValueMember" automatically remove the
 * prefixes before returning the values.
 *
 * Methods returning a pointer to the value like "getValue" cannot automatically remove
 * the prefixes. The derived classes using those methods must take this into account
 * and remove the prefixes before returning the values to the caller of the resource models.
 *
 * To access the values of the document within the models, only methods of the model classes
 * that handle the prefixes correctly should be used whenever possible. If, for whatever reason,
 * it is necessary to access the values of the documents directly via rapidjson pointer, the
 * prefixes must be taken into account and either explicitly removed or inserted.
 * For convenience if you need to directly access values within the document via rapidjson
 * pointers and need the string values of values referenced by the pointers you may use
 * the helper method "getStringValueFromPointer".
 */
// NOLINTNEXTLINE(bugprone-exception-escape)
class NumberAsStringParserDocument : public rapidjson::Document
{
/**
 * Class NumberAsStringParserWriter is the custom writer removing the prefixes for
 * strings and numbers, needs access to the protected members "prefixString" and "prefixNumber"
 * and has therefore been declared as a friend.
 */
friend class NumberAsStringParserWriter<rapidjson::StringBuffer>;

/**
 * TODO: Class ResourceBase still needs access to the prefixes.
 * Some helper methods are still missing in class NumberAsStringParserDocument.
 * See Jira Task: ....
 */
friend class ResourceBase;

public:
    [[nodiscard]]
    static bool valueIsObject(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsArray(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsNull(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsBool(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsFalse(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsTrue(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsNumber(const rapidjson::Value& value);
    [[nodiscard]]
    static bool valueIsString(const rapidjson::Value& value);

    [[nodiscard]]
    static std::string_view valueAsString(const rapidjson::Value& value);

    static rapidjson::Value createEmptyObject();

    [[nodiscard]]
    rapidjson::Value makeNumber(const std::string_view& number);
    [[nodiscard]]
    rapidjson::Value makeString(const std::string_view& str);
    [[nodiscard]]
    rapidjson::Value makeBool(bool value);

    [[nodiscard]]
    static NumberAsStringParserDocument fromJson(std::string_view json);

    [[nodiscard]]
    std::string serializeToJsonString() const;

    static std::string pointerToString(const rapidjson::Pointer& pointer);

    /**
     * Helper method removing the string and number prefixes before returning the string value.
     * Static method as no reference to the document is needed.
     * Note: Also numeric values are returned as strings (maintaining the number precision of original values).
    */
    [[nodiscard]]
    static std::string_view getStringValueFromValue(const rapidjson::Value* value);

    /**
     * Helper methods removing the string and number prefixes before returning the string value.
     * Instance method as reference to the document is needed.
     * Note: Also numeric values are returned as strings (maintaining the number precision of original values).
    */
    [[nodiscard]]
    std::string_view getStringValueFromPointer(const rapidjson::Pointer& pointer) const;

    /**
     * Sets the value of the object member specified by its key to the given value.
     * For string and numeric values the corresponding prefix is inserted.
     */
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, const rapidjson::Value& value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, const std::string_view& value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, int value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, unsigned int value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, int64_t value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, uint64_t value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, double value);

    /**
     * Sets the value of the object specified by the pointer to the given value.
     * For string and numeric values the corresponding prefix is inserted.
     */
    void setValue(const rapidjson::Pointer& pointerToEntry, const rapidjson::Value& value);
    void setValue(const rapidjson::Pointer& pointerToEntry, const std::string_view& value);
    void setValue(const rapidjson::Pointer& pointerToEntry, int value);
    void setValue(const rapidjson::Pointer& pointerToEntry, unsigned int value);
    void setValue(const rapidjson::Pointer& pointerToEntry, int64_t value);
    void setValue(const rapidjson::Pointer& pointerToEntry, uint64_t value);
    void setValue(const rapidjson::Pointer& pointerToEntry, double value);

    /**
     * Returns the string value of the given pointer by removing the string prefix.
     */
    [[nodiscard]] static std::optional<std::string_view> getOptionalStringValue(const rapidjson::Value& object,
                                                                                const rapidjson::Pointer& key);

    /**
     * Returns the number value of the object member specified by its key. Before converting the value to the number
     * the number prefix is removed.
     */
    [[nodiscard]] static std::optional<int> getOptionalIntValue(const rapidjson::Value& object,
                                                                const rapidjson::Pointer& key);
    [[nodiscard]] static std::optional<unsigned int> getOptionalUIntValue(const rapidjson::Value& object,
                                                                          const rapidjson::Pointer& key);
    [[nodiscard]] static std::optional<int64_t> getOptionalInt64Value(const rapidjson::Value& object,
                                                                      const rapidjson::Pointer& key);
    [[nodiscard]] static std::optional<uint64_t> getOptionalUInt64Value(const rapidjson::Value& object,
                                                                        const rapidjson::Pointer& key);
    [[nodiscard]] static std::optional<double> getOptionalDoubleValue(const rapidjson::Value& object,
                                                                      const rapidjson::Pointer& key);

    /**
     * Returns the string value of the object member specified by its key by removing the string prefix.
     */
    [[nodiscard]]
    std::optional<std::string_view> getOptionalStringValue(const rapidjson::Pointer& pointerToEntry) const;

    /**
     * Returns the number value of the given pointer. Before converting the value to the number
     * the number prefix is removed.
     */
    [[nodiscard]]
    std::optional<int> getOptionalIntValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]]
    std::optional<unsigned int> getOptionalUIntValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]]
    std::optional<int64_t> getOptionalInt64Value(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]]
    std::optional<uint64_t> getOptionalUInt64Value(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]]
    std::optional<double> getOptionalDoubleValue(const rapidjson::Pointer& pointerToEntry) const;

    [[nodiscard]] std::optional<std::string_view> getNumericAsString(const rapidjson::Pointer& key) const;

    [[nodiscard]] std::optional<std::string_view> findStringInArray(
        const rapidjson::Pointer& arrayPointer,
        const rapidjson::Pointer& searchKey,
        const std::string_view& searchValue,
        const rapidjson::Pointer& resultKeyPointer,
        bool ignoreValueCase = false) const;

    [[nodiscard]] std::optional<std::tuple<const rapidjson::Value*, std::size_t>> findMemberInArray(
        const rapidjson::Pointer& arrayPointer,
        const rapidjson::Pointer& searchKey,
        const std::string_view& searchValue,
        const rapidjson::Pointer& resultKeyPointer,
        bool ignoreValueCase = false) const;

    [[nodiscard]] rapidjson::Value* findMemberInArray(
        const rapidjson::Pointer& arrayPointer,
        const rapidjson::Pointer& searchKey,
        const std::string_view& searchValue);

    [[nodiscard]] const rapidjson::Value* findMemberInArray(
        const rapidjson::Pointer& arrayPointer,
        const rapidjson::Pointer& searchKey,
        const std::string_view& searchValue) const;

    [[nodiscard]] const rapidjson::Value* getMemberInArray(
        const rapidjson::Pointer& pointerToArray,
        size_t index) const;

    // Returns position of added element:
    std::size_t addToArray(const rapidjson::Pointer& pointerToArray, rapidjson::Value&& object);
    std::size_t addToArray(rapidjson::Value& array, rapidjson::Value&& object);
    void addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index,
                               ::rapidjson::Value&& key, ::rapidjson::Value&& value);

    void removeFromArray(const rapidjson::Pointer& pointerToArray, std::size_t index);
    void clearArray(const rapidjson::Pointer& pointerToArray);

    NumberAsStringParserDocument() : rapidjson::Document()
    {
    }

    /**
     * Handler method of the custom reader (this NumberAsStringParserDocument) returning the parsed string
     * by inserting the prefix to mark the value as a string value.
     */
    bool String(const Ch* text, size_t length, bool copy)
    {
        std::string s(text, length);
        s = prefixString + s;
        return rapidjson::Document::String(s.data(), static_cast<rapidjson::SizeType>(s.size()), copy);
    }

    /**
     * Handler method of the custom reader (this NumberAsStringParserDocument) returning the parsed string
     * by inserting the prefix to mark the value as a numeric value.
     */
    bool RawNumber(const Ch* text, size_t length, bool copy)
    {
        std::string s(text, length);
        s = prefixNumber + s;
        return rapidjson::Document::String(s.data(), static_cast<rapidjson::SizeType>(s.size()), copy);
    }
private:
    [[nodiscard]]
    rapidjson::Value makeValue(Ch prefix, const std::string_view& value);


    static const Ch prefixString{ '$' }; // Inserted to values to mark the values as a string.
    static const Ch prefixNumber{ '#' }; // Inserted to values to mark the values as numbers.

};

class NumberAsStringParserDocumentConverter
{
public:
    /**
     * For validation or for converting into xml a json document without the string and number
     * prefixes is necessary. The "copyToOriginalFormat" creates a copy of the given and removes
     * the prefixes.
     */
    [[nodiscard]]
    static rapidjson::Document copyToOriginalFormat(const rapidjson::Document& documentNumbersAsStrings);

    /**
     * After receiving the json string as a request and a json document without prefixes for
     * strings and numbers is created for validation. For further processing within the handler
     * the "convertToNumberAsStrings" method converts the "original" document into a document
     * containing the prefixes.
     */
    [[nodiscard]]
    static rapidjson::Document& convertToNumbersAsStrings(rapidjson::Document& document);
};

} // namespace model

namespace rapidjson
{
class CustomUtf8 : rapidjson::UTF8<char>
{
};

using BaseClass = rapidjson::GenericReader<rapidjson::UTF8<char>, rapidjson::UTF8<char>, rapidjson::CrtAllocator>;

/**
 * Generic reader which will be instantiated within the constructor of class rapidjson::Document
 * to override (replace) the "String" and "RawNumber" handler methods so that the handler methods
 * of class "NumberAsStringParserDocument" will be used instead.
 */
template<>
class GenericReader<CustomUtf8, rapidjson::UTF8<char>, rapidjson::CrtAllocator>
    : public BaseClass
{
public:

    explicit GenericReader(rapidjson::CrtAllocator* allocator = nullptr, size_t stackCapacity = 256)
        : BaseClass(allocator, stackCapacity)
    {
    }

    template <unsigned parseFlags, typename InputStream, typename Handler>
    ParseResult Parse(InputStream& is, Handler& handler)
    {
        auto result = BaseClass::Parse<parseFlags, InputStream, model::NumberAsStringParserDocument>
                        (is, reinterpret_cast<model::NumberAsStringParserDocument&>(handler));
        return result;
    }
};

} // namespace rapidjson

#endif
