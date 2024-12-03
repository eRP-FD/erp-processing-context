/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RESOURCE_BASE_HXX
#define ERP_PROCESSING_CONTEXT_RESOURCE_BASE_HXX

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <optional>

class XmlValidator;
class JsonValidator;

namespace fhirtools
{
class FhirStructureRepository;
class ValidationResults;
}


namespace model
{
template<typename>
class Extension;
class UnspecifiedResource;

// NOLINTNEXTLINE(bugprone-exception-escape)
class ResourceBase
{
public:
    explicit ResourceBase(NumberAsStringParserDocument&& document);
    virtual ~ResourceBase() = default;
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase(ResourceBase&&) noexcept = default;
    ResourceBase& operator=(const ResourceBase&) = delete;
    //NOLINTNEXTLINE(bugprone-exception-escape)
    ResourceBase& operator=(ResourceBase&&) noexcept = default;


    /**
     * Serializes the resource model into a json string.
     */
    [[nodiscard]] std::string serializeToJsonString() const;

    /**
     * Serializes the resource model into a canonical json string.
     * - Properties are alphabetically sorted.
     * - No whitespace other than single spaces in property values.
     * - The narrative (Resource.text) has to be omitted.
     * - The Resource.meta element is removed.
     */
    [[nodiscard]] std::string serializeToCanonicalJsonString() const;
    [[nodiscard]] static std::string serializeToCanonicalJsonString(const rapidjson::Value& value);

    /**
     * Serializes the resource model into a xml string.
     */
    [[nodiscard]] std::string serializeToXmlString() const;

    [[nodiscard]] const NumberAsStringParserDocument& jsonDocument() const&;
    [[nodiscard]] NumberAsStringParserDocument&& jsonDocument() &&;

    static std::string pointerToString(const rapidjson::Pointer& pointer);

    void removeEmptyObjectsAndArrays();

protected:
    ResourceBase();
    ResourceBase(const ::rapidjson::Document& resourceTemplate);

    static rapidjson::Value createEmptyObject(void);
    rapidjson::Value copyValue(const rapidjson::Value& value);
    rapidjson::Value& createArray(const rapidjson::Pointer& pointerToArray);
    rapidjson::Value& createArray(rapidjson::Value& parent, const rapidjson::Pointer& pointerToArray);

    /**
     * Sets the value of the object member specified by its key to the given value.
     */
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, const std::string_view& value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, const rapidjson::Value& value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, int value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, unsigned int value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, int64_t value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, uint64_t value);
    void setKeyValue(rapidjson::Value& object, const rapidjson::Pointer& key, double value);

    /**
     * Sets the value of the object specified by the pointer to the given value.
     */
    void setValue(const rapidjson::Pointer& pointerToEntry, const std::string_view& value);
    void setValue(const rapidjson::Pointer& pointerToEntry, const rapidjson::Value& value);
    void setValue(const rapidjson::Pointer& pointerToEntry, int value);
    void setValue(const rapidjson::Pointer& pointerToEntry, unsigned int value);
    void setValue(const rapidjson::Pointer& pointerToEntry, int64_t value);
    void setValue(const rapidjson::Pointer& pointerToEntry, uint64_t value);
    void setValue(const rapidjson::Pointer& pointerToEntry, double value);

    /**
     * Returns the string value of the given pointer.
     */
    [[nodiscard]] std::string_view getStringValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<std::string_view>
    getOptionalStringValue(const rapidjson::Pointer& pointerToEntry) const;
    /**
     * Returns the string value of the object member specified by its key.
     */
    [[nodiscard]] std::string_view getStringValue(const rapidjson::Value& object, const rapidjson::Pointer& key) const;
    [[nodiscard]] std::optional<std::string_view> getOptionalStringValue(const rapidjson::Value& object,
                                                                         const rapidjson::Pointer& key) const;

    /**
     * Returns the integer value of the given pointer.
     */
    [[nodiscard]] int getIntValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<int> getOptionalIntValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] unsigned int getUIntValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<unsigned int> getOptionalUIntValue(const rapidjson::Pointer& pointerToEntry) const;

    /**
     * Returns the integer value of the given pointer.
     */
    [[nodiscard]] int64_t getInt64Value(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<int64_t> getOptionalInt64Value(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] uint64_t getUInt64Value(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<uint64_t> getOptionalUInt64Value(const rapidjson::Pointer& pointerToEntry) const;

    /**
     * Returns the double value of the given pointer.
     */
    [[nodiscard]] double getDoubleValue(const rapidjson::Pointer& pointerToEntry) const;
    [[nodiscard]] std::optional<double> getOptionalDoubleValue(const rapidjson::Pointer& pointerToEntry) const;

    [[nodiscard]] std::optional<std::string_view> getNumericAsString(const rapidjson::Pointer& pointerToEntry) const;

    void removeElement(const rapidjson::Pointer& pointerToEntry);

    [[nodiscard]] bool hasValue(const rapidjson::Pointer& pointerToValue) const;

    /**
     * Returns the value of the specified child member of the given value.
     * If the given value does not have a child with the specified "valueMemberName" an exception is thrown.
     */
    [[nodiscard]] static const rapidjson::Value& getValueMember(const rapidjson::Value& value,
                                                                std::string_view valueMemberName);

    /**
     * Returns a pointer to the value for the given rapidjson pointer.
     */
    [[nodiscard]] const rapidjson::Value* getValue(const rapidjson::Pointer& pointer) const;
    [[nodiscard]] rapidjson::Value* getValue(const rapidjson::Pointer& pointer);

    [[nodiscard]] static const rapidjson::Value* getValue(const rapidjson::Value& parent,
                                                          const rapidjson::Pointer& pointer);

    // Array helpers below
    // -------------------
    /**
     * Search for an object in an array which has a member key value pair `searchKey`/`searchValue`.
     * For the first match, return its value for the `resultKey`.
     * Return an empty Value otherwise.
     *
     * In XPath this would be equivalent to /element[@searchKey=searchValue]/@resultKey
     */
    [[nodiscard]] std::optional<std::string_view> findStringInArray(const rapidjson::Pointer& array,
                                                                    const rapidjson::Pointer& searchKey,
                                                                    const std::string_view& searchValue,
                                                                    const rapidjson::Pointer& resultKeyPointer,
                                                                    bool ignoreValueCase = false) const;

    [[nodiscard]] std::optional<std::tuple<const rapidjson::Value*, std::size_t>>
    findMemberInArray(const rapidjson::Pointer& arrayPointer, const rapidjson::Pointer& searchKey,
                      const std::string_view& searchValue, const rapidjson::Pointer& resultKeyPointer,
                      bool ignoreValueCase = false) const;

    [[nodiscard]] rapidjson::Value* findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                      const rapidjson::Pointer& searchKey,
                                                      const std::string_view& searchValue);

    [[nodiscard]] const rapidjson::Value* findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                            const rapidjson::Pointer& searchKey,
                                                            const std::string_view& searchValue) const;

    [[nodiscard]] const rapidjson::Value* getMemberInArray(const rapidjson::Pointer& pointerToArray,
                                                           size_t index) const;

    // Returns position of added element:
    std::size_t addToArray(const rapidjson::Pointer& pointerToArray, std::string_view str);
    std::size_t addToArray(const rapidjson::Pointer& pointerToArray, rapidjson::Value&& object);
    std::size_t addToArray(rapidjson::Value& array, rapidjson::Value&& object);
    std::size_t addStringToArray(rapidjson::Value& array, std::string_view str);

    void addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index, ::std::string_view key,
                               ::std::string_view value);
    void addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index, ::std::string_view key,
                               ::std::initializer_list<::std::string_view> values);

    void removeFromArray(const rapidjson::Pointer& pointerToArray, std::size_t index);
    void clearArray(const rapidjson::Pointer& pointerToArray);

    [[nodiscard]] size_t valueSize(const rapidjson::Pointer& pointerToEntry) const;

private:
    NumberAsStringParserDocument mJsonDocument;
};
//
}

#endif//ERP_PROCESSING_CONTEXT_RESOURCE_BASE_HXX
