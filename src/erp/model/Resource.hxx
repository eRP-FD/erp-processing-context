#ifndef ERP_PROCESSING_CONTEXT_RESOURCE_HXX
#define ERP_PROCESSING_CONTEXT_RESOURCE_HXX

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/validation/SchemaType.hxx"

#include <optional>

class XmlValidator;

namespace model
{
class ResourceBase
{
public:
    virtual ~ResourceBase() = default;
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase(ResourceBase&&) noexcept = default;
    ResourceBase& operator=(const ResourceBase&) = delete;
    ResourceBase& operator=(ResourceBase&&) noexcept = default;

    void validate() const;

    /**
     * Serializes the resource model into a json string.
     */
    [[nodiscard]] std::string serializeToJsonString() const;
    [[nodiscard]] static std::string serializeToJsonString(const rapidjson::Value& value);

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

    [[nodiscard]] const NumberAsStringParserDocument& jsonDocument() const;

    static std::string pointerToString(const rapidjson::Pointer& pointer);

protected:
    ResourceBase();
    explicit ResourceBase(NumberAsStringParserDocument&& document);

    // may be overwritten by derived classes to provide specific validation
    virtual void doValidate(const rapidjson::Document& document) const;

    // use a JSON template to initialize the json tree
    void initFromTemplate(const rapidjson::Document& tpl);

    static rapidjson::Value createEmptyObject (void);
    rapidjson::Value copyValue(const rapidjson::Value& value);

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
    [[nodiscard]] std::optional<std::string_view> getOptionalStringValue(const rapidjson::Pointer& pointerToEntry) const;

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

    void removeElement(const rapidjson::Pointer& pointerToEntry);

    [[nodiscard]] bool hasValue(const rapidjson::Pointer& pointerToValue) const;

    /**
     * Returns the value of the specified child member of the given value.
     * If the given value does not have a child with the specified "valueMemberName" an exception is thrown.
     */
    [[nodiscard]] static const rapidjson::Value& getValueMember(const rapidjson::Value& value, std::string_view valueMemberName);

    /**
     * Returns a pointer to the value for the given rapidjson pointer.
     */
    [[nodiscard]] const rapidjson::Value* getValue(const rapidjson::Pointer& pointer) const;
    [[nodiscard]] rapidjson::Value* getValue(const rapidjson::Pointer& pointer);


    // Array helpers below
    // -------------------
    /**
     * Search for an object in an array which has a member key value pair `searchKey`/`searchValue`.
     * For the first match, return its value for the `resultKey`.
     * Return an empty Value otherwise.
     *
     * In XPath this would be equivalent to /element[@searchKey=searchValue]/@resultKey
     */
    [[nodiscard]] std::optional<std::string_view> findStringInArray (
        const rapidjson::Pointer& array,
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

    [[nodiscard]] rapidjson::Value* getMemberInArray(const rapidjson::Pointer& pointerToArray, size_t index);

    // Returns position of added element:
    std::size_t addToArray (const rapidjson::Pointer& pointerToArray, rapidjson::Value&& object);

    void removeFromArray(const rapidjson::Pointer& pointerToArray, std::size_t index);
    void clearArray(const rapidjson::Pointer& pointerToArray);

    [[nodiscard]] size_t valueSize(const rapidjson::Pointer& pointerToEntry) const;

private:
    NumberAsStringParserDocument mJsonDocument;
};


template<class TDerivedModel>
class Resource : public ResourceBase
{
public:
    static TDerivedModel fromXmlNoValidation(std::string_view xml);
    static TDerivedModel fromXml(std::string_view xml, const XmlValidator& validator, SchemaType schemaType);
    static TDerivedModel fromJson(std::string_view json);
    static TDerivedModel fromJson(const rapidjson::Value& json);
    static TDerivedModel fromJson(model::NumberAsStringParserDocument&& json);

protected:
    Resource() = default;
    explicit Resource(NumberAsStringParserDocument&& document);
};

extern template class Resource<class AuditEvent>;
extern template class Resource<class AuditMetaData>;
extern template class Resource<class Binary>;
extern template class Resource<class Bundle>;
extern template class Resource<class Composition>;
extern template class Resource<class Communication>;
extern template class Resource<class Device>;
extern template class Resource<class MedicationDispense>;
extern template class Resource<class MetaData>;
extern template class Resource<class Parameters>;
extern template class Resource<class Patient>;
extern template class Resource<class Signature>;
extern template class Resource<class Task>;
}

#endif//ERP_PROCESSING_CONTEXT_RESOURCE_HXX
