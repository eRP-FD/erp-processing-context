/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_RESOURCE_HXX
#define ERP_PROCESSING_CONTEXT_RESOURCE_HXX

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/validation/SchemaType.hxx"

#include <optional>
#include <variant>

class XmlValidator;
class JsonValidator;
class InCodeValidator;


namespace model
{
class Extension;
class UnspecifiedResource;

class ResourceBase
{
public:
    struct NoProfileTag {
    };
    static constexpr auto NoProfile = NoProfileTag{};
    using Profile = ::std::variant<::std::string_view, NoProfileTag>;

    virtual ~ResourceBase() = default;
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase(ResourceBase&&) noexcept = default;
    ResourceBase& operator=(const ResourceBase&) = delete;
    ResourceBase& operator=(ResourceBase&&) noexcept = default;

    template <typename ExtensionT = model::Extension>
    std::optional<ExtensionT> getExtension(const std::string_view& url = ExtensionT::url) const;

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

    [[nodiscard]] const NumberAsStringParserDocument& jsonDocument() const;

    static std::string pointerToString(const rapidjson::Pointer& pointer);


    [[nodiscard]] std::optional<std::string_view> identifierUse() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingCode() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierValue() const;

protected:
    explicit ResourceBase(Profile profile);
    ResourceBase(Profile profile, const ::rapidjson::Document& resourceTemplate);
    explicit ResourceBase(NumberAsStringParserDocument&& document);

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
     * Returns the string value of the object member specified by its key.
     */
    [[nodiscard]] std::string_view getStringValue(const rapidjson::Value& object, const rapidjson::Pointer& key) const;
    [[nodiscard]]
    std::optional<std::string_view> getOptionalStringValue(const rapidjson::Value& object, const rapidjson::Pointer& key) const;

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
    [[nodiscard]] static const rapidjson::Value& getValueMember(const rapidjson::Value& value, std::string_view valueMemberName);

    /**
     * Returns a pointer to the value for the given rapidjson pointer.
     */
    [[nodiscard]] const rapidjson::Value* getValue(const rapidjson::Pointer& pointer) const;
    [[nodiscard]] rapidjson::Value* getValue(const rapidjson::Pointer& pointer);

    [[nodiscard]] static const rapidjson::Value* getValue(const rapidjson::Value& parent, const rapidjson::Pointer& pointer);

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

    [[nodiscard]] const rapidjson::Value* findMemberInArray(
        const rapidjson::Pointer& arrayPointer,
        const rapidjson::Pointer& searchKey,
        const std::string_view& searchValue) const;

    [[nodiscard]] const rapidjson::Value* getMemberInArray(
        const rapidjson::Pointer& pointerToArray,
        size_t index) const;

    // Returns position of added element:
    std::size_t addToArray (const rapidjson::Pointer& pointerToArray, std::string_view str);
    std::size_t addToArray (const rapidjson::Pointer& pointerToArray, rapidjson::Value&& object);
    std::size_t addToArray (rapidjson::Value& array, rapidjson::Value&& object);
    std::size_t addStringToArray (rapidjson::Value& array, std::string_view str);

    void addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index, ::std::string_view key,
                               ::std::string_view value);
    void addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index, ::std::string_view key,
                               ::std::initializer_list<::std::string_view> values);

    void removeFromArray(const rapidjson::Pointer& pointerToArray, std::size_t index);
    void clearArray(const rapidjson::Pointer& pointerToArray);

    [[nodiscard]] size_t valueSize(const rapidjson::Pointer& pointerToEntry) const;

    std::optional<UnspecifiedResource> extractUnspecifiedResource(const rapidjson::Pointer& path) const;


private:
    std::optional<NumberAsStringParserDocument> getExtensionDocument(const std::string_view& url) const;

    NumberAsStringParserDocument mJsonDocument;
};


template<class TDerivedModel, typename SchemaVersionType = ResourceVersion::DeGematikErezeptWorkflowR4>
class Resource : public ResourceBase
{
public:
    static TDerivedModel fromXmlNoValidation(std::string_view xml);
    static TDerivedModel fromXml(std::string_view xml, const XmlValidator& validator,
                                 const InCodeValidator& inCodeValidator, SchemaType schemaType,
                                 std::optional<SchemaVersionType> enforcedVersion = {});
    static TDerivedModel fromJsonNoValidation(std::string_view json);
    static TDerivedModel fromJson(std::string_view json, const JsonValidator& jsonValidator,
                                  const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
                                  SchemaType schemaType);
    static TDerivedModel fromJson(const rapidjson::Value& json);
    static TDerivedModel fromJson(model::NumberAsStringParserDocument&& json);

    SchemaVersionType getSchemaVersion() const;

protected:
    using ResourceBase::ResourceBase;

private:
    void doValidation(std::string_view xml, const XmlValidator& validator, const InCodeValidator& inCodeValidator,
                         SchemaType schemaType, std::optional<SchemaVersionType> enforcedVersion = {}) const;

    friend class ResourceBase;
};

class UnspecifiedResource : public Resource<UnspecifiedResource>
{
private:
    friend Resource<UnspecifiedResource>;
    explicit UnspecifiedResource(NumberAsStringParserDocument&& document);
public:
    [[nodiscard]] std::string_view getResourceType() const;
};

template<typename ExtensionT>
std::optional<ExtensionT> ResourceBase::getExtension(const std::string_view& url) const
{
    static_assert(std::is_base_of_v<::model::Extension, ExtensionT> ||
                  std::is_same_v<::model::Extension, ExtensionT>,
                  "ExtensionT must be derived from model::Extension");
    auto extensionNode = getExtensionDocument(url);
    if (extensionNode)
    {
        return ExtensionT{std::move(*extensionNode)};
    }
    return std::nullopt;
}


extern template class Resource<class AuditEvent>;
extern template class Resource<class AuditMetaData>;
extern template class Resource<class Binary>;
extern template class Resource<class Bundle>;
extern template class Resource<class ChargeItem>; //NOLINT(bugprone-forward-declaration-namespace)
extern template class Resource<class Composition>;
extern template class Resource<class Communication>; //NOLINT(bugprone-forward-declaration-namespace)
extern template class Resource<class Consent>;
extern template class Resource<class Device>;
extern template class Resource<class Dosage, ResourceVersion::KbvItaErp>;
extern template class Resource<class ErxReceipt>;
extern template class Resource<class Extension>;
extern template class Resource<class KbvBundle, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvComposition, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvCoverage, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationGeneric, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationCompounding, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationFreeText, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationIngredient, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationModelHelper, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationPzn, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvMedicationRequest, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvOrganization, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvPractitioner, ResourceVersion::KbvItaErp>;
extern template class Resource<class KbvPracticeSupply, ResourceVersion::KbvItaErp>;
extern template class Resource<class MedicationDispense>; //NOLINT(bugprone-forward-declaration-namespace)
extern template class Resource<class MedicationDispenseBundle, ResourceVersion::NotProfiled>;
extern template class Resource<class MetaData>;
extern template class Resource<class OperationOutcome>;
extern template class Resource<class Parameters>; //NOLINT(bugprone-forward-declaration-namespace)
extern template class Resource<class Patient, ResourceVersion::KbvItaErp>;
extern template class Resource<class Reference>;
extern template class Resource<class Signature>;
extern template class Resource<class Subscription>;
extern template class Resource<class Task>;
extern template class Resource<class UnspecifiedResource>;
}

#endif//ERP_PROCESSING_CONTEXT_RESOURCE_HXX
