/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Resource.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirCanonicalizer.hxx"
#include "erp/fhir/FhirConverter.hxx"
#include "erp/fhir/internal/FhirSAXHandler.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/model/AuditEvent.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvComposition.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/model/OperationOutcome.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "fhirtools/util/SaxHandler.hxx"

#include <boost/format.hpp>
#include <fhirtools/model/erp/ErpElement.hxx>
#include <fhirtools/validator/FhirPathValidator.hxx>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#ifdef _WIN32
#pragma warning(                                                                                                       \
    disable : 4127)// suppress warning C4127: Conditional expression is constant (rapidjson::Pointer::Stringify)
#endif

namespace model
{

namespace rj = rapidjson;

namespace
{
const auto profilePointer = ::rapidjson::Pointer{"/meta/profile/0"};

rj::GenericStringRef<std::string_view::value_type> stringRefFrom(const std::string_view& s)
{
    return rj::StringRef(s.data(), s.size());
}

} // namespace

std::string ResourceBase::serializeToJsonString() const
{
    return mJsonDocument.serializeToJsonString();
}


std::string ResourceBase::serializeToCanonicalJsonString() const
{
    return serializeToCanonicalJsonString(jsonDocument());
}

std::string ResourceBase::serializeToCanonicalJsonString(const rj::Value& value)
{
    return FhirCanonicalizer::serialize(value);
}

std::string ResourceBase::serializeToXmlString() const
{
    return Fhir::instance().converter().jsonToXmlString(mJsonDocument);
}

const NumberAsStringParserDocument& ResourceBase::jsonDocument() const &
{
    return mJsonDocument;
}

NumberAsStringParserDocument&& model::ResourceBase::jsonDocument() &&
{
    return std::move(mJsonDocument);
}

ResourceBase::ResourceBase(Profile profile)
    : mJsonDocument()
{
    mJsonDocument.SetObject();
    if (::std::holds_alternative<::std::string_view>(profile))
    {
        setValue(profilePointer, ResourceVersion::versionizeProfile(::std::get<::std::string_view>(profile)));
    }
}

ResourceBase::ResourceBase(Profile profile, const ::rapidjson::Document& resourceTemplate)
    : mJsonDocument()
{
    mJsonDocument.SetObject();
    mJsonDocument.CopyFrom(resourceTemplate, mJsonDocument.GetAllocator());
    if (::std::holds_alternative<::std::string_view>(profile))
    {
        setValue(profilePointer, ResourceVersion::versionizeProfile(::std::get<::std::string_view>(profile)));
    }
}

ResourceBase::ResourceBase(NumberAsStringParserDocument&& document)
    : mJsonDocument(std::move(document))
{
}

std::string ResourceBase::pointerToString(const rapidjson::Pointer& pointer)
{
    return NumberAsStringParserDocument::pointerToString(pointer);
}

rj::Value ResourceBase::createEmptyObject()
{
    return NumberAsStringParserDocument::createEmptyObject();
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, const std::string_view& value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, const rj::Value& value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, int value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, unsigned int value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, int64_t value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, uint64_t value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, double value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, const std::string_view& value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, const rapidjson::Value& value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, int value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, unsigned int value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, int64_t value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, uint64_t value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, double value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::removeElement(const rapidjson::Pointer& pointerToEntry)
{
    pointerToEntry.Erase(mJsonDocument);
}

rapidjson::Value ResourceBase::copyValue(const rapidjson::Value& value)
{
    rapidjson::Value v;
    v.CopyFrom(value, mJsonDocument.GetAllocator());
    return v;
}

std::size_t ResourceBase::addToArray(const rapidjson::Pointer& pointerToArray, std::string_view str)
{
    return mJsonDocument.addToArray(pointerToArray, mJsonDocument.makeString(str));
}

std::size_t ResourceBase::addToArray(const rj::Pointer& pointerToArray, rapidjson::Value&& object)
{
    return mJsonDocument.addToArray(pointerToArray, std::move(object));
}

std::size_t ResourceBase::addToArray(rapidjson::Value& array, rapidjson::Value&& object)
{
    return mJsonDocument.addToArray(array, std::move(object));
}

std::size_t ResourceBase::addStringToArray(rapidjson::Value& array, std::string_view str)
{
    return addToArray(array, mJsonDocument.makeString(str));
}

void ResourceBase::addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index,
                                         ::std::string_view key, ::std::string_view value)
{
    mJsonDocument.addMemberToArrayEntry(pointerToArray, index, ::rapidjson::Value{stringRefFrom(key)},
                                        mJsonDocument.makeString(value));
}

void ResourceBase::addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index,
                                         ::std::string_view key, ::std::initializer_list<::std::string_view> values)
{
    auto array = ::rapidjson::Value{::rapidjson::Type::kArrayType};
    for (const auto& value : values)
    {
        array.PushBack(mJsonDocument.makeString(value), mJsonDocument.GetAllocator());
    }

    mJsonDocument.addMemberToArrayEntry(pointerToArray, index, ::rapidjson::Value{stringRefFrom(key)},
                                        ::std::move(array));
}

void ResourceBase::removeFromArray(const rj::Pointer& pointerToArray, std::size_t index)
{
    return mJsonDocument.removeFromArray(pointerToArray, index);
}

void ResourceBase::clearArray(const rj::Pointer& pointerToArray)
{
    return mJsonDocument.clearArray(pointerToArray);
}

std::string_view ResourceBase::getStringValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto str = getOptionalStringValue(pointerToEntry);
    ModelExpect(str.has_value(), pointerToString(pointerToEntry) + ": entry not present or not a string");
    return str.value();
}

std::optional<std::string_view> ResourceBase::getOptionalStringValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalStringValue(pointerToEntry);
}

std::string_view ResourceBase::getStringValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto str = getOptionalStringValue(object, key);
    ModelExpect(str.has_value(), pointerToString(key) + ": entry not present or not a string");
    return str.value();
}

std::optional<std::string_view> ResourceBase::getOptionalStringValue(const rj::Value& object,
                                                                     const rj::Pointer& key) const
{
    return NumberAsStringParserDocument::getOptionalStringValue(object, key);
}

int ResourceBase::getIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalIntValue(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an int");
    return i.value();
}

std::optional<int> ResourceBase::getOptionalIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalIntValue(pointerToEntry);
}

unsigned int ResourceBase::getUIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalUIntValue(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an unsigned int");
    return i.value();
}

std::optional<unsigned int> ResourceBase::getOptionalUIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalUIntValue(pointerToEntry);
}

int64_t ResourceBase::getInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalInt64Value(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an int64");
    return i.value();
}

std::optional<int64_t> ResourceBase::getOptionalInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalInt64Value(pointerToEntry);
}

uint64_t ResourceBase::getUInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalUInt64Value(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an unsigned int64");
    return i.value();
}

std::optional<uint64_t> ResourceBase::getOptionalUInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalUInt64Value(pointerToEntry);
}

double ResourceBase::getDoubleValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto f = getOptionalDoubleValue(pointerToEntry);
    ModelExpect(f.has_value(), pointerToString(pointerToEntry) + ": entry not present or not a double");
    return f.value();
}

std::optional<double> ResourceBase::getOptionalDoubleValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalDoubleValue(pointerToEntry);
}

std::optional<std::string_view> ResourceBase::getNumericAsString(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getNumericAsString(pointerToEntry);
}

const rapidjson::Value& ResourceBase::getValueMember(const rapidjson::Value& value, std::string_view valueMemberName)
{
    const auto valueMember = value.FindMember(stringRefFrom(valueMemberName));
    ModelExpect(valueMember != value.MemberEnd(), "valueMember does not exist: " + std::string(valueMemberName));
    return valueMember->value;
}

const rapidjson::Value* ResourceBase::getValue(const rapidjson::Pointer& pointer) const
{
    return pointer.Get(mJsonDocument);
}

rapidjson::Value* ResourceBase::getValue(const rapidjson::Pointer& pointer)
{
    return pointer.Get(mJsonDocument);
}

const rapidjson::Value* ResourceBase::getValue(const rapidjson::Value& parent, const rapidjson::Pointer& pointer)
{
    return pointer.Get(parent);
}

std::optional<std::string_view> ResourceBase::findStringInArray(const rj::Pointer& arrayPointer,
                                                                const rj::Pointer& searchKey,
                                                                const std::string_view& searchValue,
                                                                const rj::Pointer& resultKeyPointer,
                                                                bool ignoreValueCase) const
{
    return mJsonDocument.findStringInArray(arrayPointer, searchKey, searchValue, resultKeyPointer, ignoreValueCase);
}

std::optional<std::tuple<const rapidjson::Value*, std::size_t>>
ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer, const rapidjson::Pointer& searchKey,
                                const std::string_view& searchValue, const rapidjson::Pointer& resultKeyPointer,
                                bool ignoreValueCase) const
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue, resultKeyPointer, ignoreValueCase);
}

rapidjson::Value* ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                  const rapidjson::Pointer& searchKey,
                                                  const std::string_view& searchValue)
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue);
}

const rapidjson::Value* ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                        const rapidjson::Pointer& searchKey,
                                                        const std::string_view& searchValue) const
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue);
}

const rapidjson::Value* ResourceBase::getMemberInArray(const rapidjson::Pointer& pointerToArray, size_t index) const
{
    return mJsonDocument.getMemberInArray(pointerToArray, index);
}

bool ResourceBase::hasValue(const rapidjson::Pointer& pointerToValue) const
{
    return pointerToValue.Get(mJsonDocument) != nullptr;
}

size_t ResourceBase::valueSize(const rapidjson::Pointer& pointerToEntry) const
{
    const auto* resources = pointerToEntry.Get(mJsonDocument);
    if (resources == nullptr)
        return 0;
    else
        return resources->Size();
}

std::optional<UnspecifiedResource> ResourceBase::extractUnspecifiedResource(const rapidjson::Pointer& path) const
{
    const auto* entry = getValue(path);
    if (entry)
    {
        return UnspecifiedResource::fromJson(*entry);
    }
    return std::nullopt;
}


std::optional<NumberAsStringParserDocument> ResourceBase::getExtensionDocument(const std::string_view& url) const
{
    static const rapidjson::Pointer extensionPtr(resource::ElementName::path(resource::elements::extension));
    static const rapidjson::Pointer urlPtr(resource::ElementName::path(resource::elements::url));
    const auto* extensionNode = mJsonDocument.findMemberInArray(extensionPtr, urlPtr, url);
    if (extensionNode == nullptr)
    {
        return std::nullopt;
    }
    std::optional<NumberAsStringParserDocument> extension{std::in_place};
    extension->CopyFrom(*extensionNode, extension->GetAllocator());
    return extension;
}

std::optional<std::string_view> ResourceBase::identifierUse() const
{
    static const rapidjson::Pointer identifierUsePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::use));
    return getOptionalStringValue(identifierUsePointer);
}

std::optional<std::string_view> ResourceBase::identifierTypeCodingCode() const
{
    static const rapidjson::Pointer identifierTypeCodingCodePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(identifierTypeCodingCodePointer);
}

std::optional<std::string_view> ResourceBase::identifierTypeCodingSystem() const
{
    static const rapidjson::Pointer identifierTypeCodingSystemPointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::system));
    return getOptionalStringValue(identifierTypeCodingSystemPointer);
}

std::optional<std::string_view> ResourceBase::identifierSystem() const
{
    static const rapidjson::Pointer identifierSystemPointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::system));
    return getOptionalStringValue(identifierSystemPointer);
}

std::optional<std::string_view> ResourceBase::identifierValue() const
{
    static const rapidjson::Pointer identifierValuePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::value));
    return getOptionalStringValue(identifierValuePointer);
}

SchemaType ResourceBase::getProfile() const
{
    const auto& profileString = getProfileName();
    ModelExpect(profileString.has_value(), "Cannot determine SchemaType without profile name");
    const auto& parts = String::split(*profileString, '|');
    const auto& profileWithoutVersion = parts[0];
    const auto& pathParts = String::split(profileWithoutVersion, '/');
    const auto& profile = pathParts.back();
    const auto& schemaType = magic_enum::enum_cast<SchemaType>(profile);
    ModelExpect(schemaType.has_value(),
                "Could not extract schema type from profile string: " + std::string(*profileString));
    return *schemaType;
}

std::optional<std::string_view> ResourceBase::getProfileName() const
{
    static const rj::Pointer profileArrayPtr("/meta/profile");
    const auto* metaArray = getValue(profileArrayPtr);
    if (! (metaArray && metaArray->IsArray()))
    {
        return std::nullopt;
    }
    ModelExpect(metaArray->Size() == 1, "/meta/profile array must have size 1");
    return getStringValue(profilePointer);
}


// ---

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromXmlNoValidation(std::string_view xmlDocument)
{
    try
    {
        return TDerivedModel::fromJson(Fhir::instance().converter().xmlStringToJson(xmlDocument));
    }
    catch (const ErpException&)
    {
        throw;
    }
    catch (const ModelException&)
    {
        throw;
    }
    catch (const std::runtime_error& e)
    {
        std::throw_with_nested(ModelException(e.what()));
    }
}

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromXml(
    std::string_view xml, const XmlValidator& validator, const InCodeValidator& inCodeValidator, SchemaType schemaType,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles, bool validateGeneric,
    std::optional<SchemaVersionType> enforcedVersion, SchemaVersionType fallbackVersion)
{
    using ResourceFactory = model::ResourceFactory<TDerivedModel>;
    typename ResourceFactory::Options options;
    options.fallbackVersion = fallbackVersion;
    options.enforcedVersion = enforcedVersion;
    if (! validateGeneric)
    {
        options.genericValidationMode = Configuration::GenericValidationMode::disable;
    }
    return ResourceFactory::fromXml(xml, validator, std::move(options))
        .getValidated(schemaType, validator, inCodeValidator, supportedBundles);
}

template<class TDerivedModel, typename SchemaVersionType>
void Resource<TDerivedModel, SchemaVersionType>::additionalValidation() const
{
}


template<class TDerivedModel, typename SchemaVersionType>
fhirtools::ValidationResults
Resource<TDerivedModel, SchemaVersionType>::genericValidate(model::ResourceVersion::FhirProfileBundleVersion version,
                                                            const fhirtools::ValidatorOptions& options) const
{
    std::string resourceTypeName;
    if constexpr (std::is_same_v<TDerivedModel, UnspecifiedResource>)
    {
        resourceTypeName = ResourceBase::getResourceType();
    }
    else
    {
        resourceTypeName = TDerivedModel::resourceTypeName;
    }
    auto fhirPathElement =
        std::make_shared<ErpElement>(&Fhir::instance().structureRepository(version),
                                     std::weak_ptr<const fhirtools::Element>{}, resourceTypeName,
                                     &ResourceBase::jsonDocument());
    std::ostringstream profiles;
    std::string_view sep;
    for (const auto& prof: fhirPathElement->profiles())
    {
        profiles << sep << prof;
        sep = ", ";
    }
    auto timer = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryFhirValidation, "Generic FHIR Validation",
        {{"resourceType", resourceTypeName}, {std::string{"profiles"}, profiles.str()}});

    return fhirtools::FhirPathValidator::validate(fhirPathElement, resourceTypeName, options);
}

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromJsonNoValidation(std::string_view json)
{
    return TDerivedModel(std::move(NumberAsStringParserDocument::fromJson(json)));
}

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromJson(
    std::string_view json, const JsonValidator& jsonValidator, const XmlValidator& xmlValidator,
    const InCodeValidator& inCodeValidator, SchemaType schemaType,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles,
    bool validateGeneric, SchemaVersionType fallbackVersion)
{
    using ResourceFactory = model::ResourceFactory<TDerivedModel>;
    typename ResourceFactory::Options options;
    options.fallbackVersion = fallbackVersion;
    if (! validateGeneric)
    {
        options.genericValidationMode = Configuration::GenericValidationMode::disable;
    }
    return ResourceFactory::fromJson(json, jsonValidator, std::move(options))
        .getValidated(schemaType, xmlValidator, inCodeValidator, supportedBundles);

}

template<typename TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromJson(const rj::Value& json)
{
    NumberAsStringParserDocument document;
    document.CopyFrom(json, document.GetAllocator());
    return TDerivedModel(std::move(document));
}

template<typename TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromJson(model::NumberAsStringParserDocument&& json)
{
    return TDerivedModel(std::move(json));
}

template<typename TDerivedModel, typename SchemaVersionType>
std::optional<SchemaVersionType> Resource<TDerivedModel, SchemaVersionType>::getSchemaVersion() const
{
    return ResourceBase::getSchemaVersion<SchemaVersionType>();
}

namespace {
std::optional<ResourceVersion::FhirProfileBundleVersion>
    parameterCreateTaskBundleVersion(const NumberAsStringParserDocument& jsonDocument)
{
    using namespace resource;
    rapidjson::Pointer parameterPtr{ElementName::path(elements::parameter)};
    rapidjson::Pointer namePtr{ElementName::path(elements::name)};
    rapidjson::Pointer systemPtr{ElementName::path(elements::valueCoding, elements::system)};
    auto workflowTypeParameter = jsonDocument.findMemberInArray(parameterPtr, namePtr, "workflowType", systemPtr);
    if (! workflowTypeParameter)
    {
        return {};
    }
    const auto* workflowTypeValue = get<const rapidjson::Value*>(*workflowTypeParameter);
    if (workflowTypeValue && NumberAsStringParserDocument::valueIsString(*workflowTypeValue))
    {
        const auto workflowType = NumberAsStringParserDocument::valueAsString(*workflowTypeValue);
        if (workflowType == code_system::flowType)
        {
            return ResourceVersion::FhirProfileBundleVersion::v_2023_07_01;
        }
        if (workflowType == code_system::deprecated::flowType)
        {
            return ResourceVersion::FhirProfileBundleVersion::v_2022_01_01;
        }
    }
    return std::nullopt;
}

std::optional<ResourceVersion::FhirProfileBundleVersion>
    parameterActivateTaskBundleVersion(const NumberAsStringParserDocument& jsonDocument)
{
    using namespace resource;
    rapidjson::Pointer parameterPtr{ElementName::path(elements::parameter)};
    rapidjson::Pointer namePtr{ElementName::path(elements::name)};
    rapidjson::Pointer systemPtr{ElementName::path(elements::valueCoding, elements::system)};
    auto ePrescriptionParameter = jsonDocument.findMemberInArray(parameterPtr, namePtr, "ePrescription", systemPtr);
    if (ePrescriptionParameter)
    {
        if (ResourceVersion::supportedBundles().contains(ResourceVersion::FhirProfileBundleVersion::v_2022_01_01))
        {
            // return old version if supported, to allow XSD validation, which is stricter
            return ResourceVersion::FhirProfileBundleVersion::v_2022_01_01;
        }
        return ResourceVersion::FhirProfileBundleVersion::v_2023_07_01;
    }
    return std::nullopt;
}

const ResourceVersion::ProfileInfo* getProfileInfoFromProfileArray(const rapidjson::Value::ConstArray& profileArray)
{
    for (const auto& profileValue: profileArray)
    {
        if (model::NumberAsStringParserDocument::valueIsString(profileValue))
        {
            const auto profileName = model::NumberAsStringParserDocument::valueAsString(profileValue);
            const auto *profInfo = ResourceVersion::profileInfoFromProfileName(profileName);
            if (profInfo)
            {
                return profInfo;
            }
        }
    }
    return nullptr;
}

const ResourceVersion::ProfileInfo* getProfileInfoFromResource(const rapidjson::Value& resource)
{
    using namespace resource;
    static rapidjson::Pointer metaProfilePtr(
            ElementName::path(elements::meta, elements::profile));
    const auto* metaProfileValue = metaProfilePtr.Get(resource);
    if (! metaProfileValue || ! metaProfileValue->IsArray())
    {
        return nullptr;
    }
    const auto profileArray = metaProfileValue->GetArray();
    return getProfileInfoFromProfileArray(profileArray);
}

const ResourceVersion::ProfileInfo* getProfileInfoFromBundleEntries(const rapidjson::Value::ConstArray& entriesArray)
{
    using namespace resource;
    static rapidjson::Pointer resourcePtr(ElementName::path(elements::resource));
    for (const auto& entryValue : entriesArray)
    {
        const auto* resourceValue = resourcePtr.Get(entryValue);
        if (! resourceValue || ! resourceValue->IsObject())
        {
            continue;
        }
        const auto* profInfo = getProfileInfoFromResource(*resourceValue);
        if (profInfo)
        {
            return profInfo;
        }
    }
    return nullptr;
}

const ResourceVersion::ProfileInfo* getProfileInfoFromBundle(const NumberAsStringParserDocument& jsonDocument)
{
    using namespace resource;
    static rapidjson::Pointer entryPtr{ElementName::path(elements::entry)};
    const rapidjson::Value* entries = entryPtr.Get(jsonDocument);
    if (entries)
    {
        ModelExpect(entries->IsArray(), "Bundle.entry is not an array.");
        const auto entriesArray = entries->GetArray();
        const auto* profInfo = getProfileInfoFromBundleEntries(entriesArray);
        if (profInfo)
        {
            return profInfo;
        }

    }
    return nullptr;
}
}

std::optional<ResourceVersion::FhirProfileBundleVersion> ResourceBase::fhirProfileBundleVersion(
    const std::set<ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const
{
    auto profileName = getProfileName();
    if (profileName)
    {
        const auto *profInfo = ResourceVersion::profileInfoFromProfileName(*profileName);

        return profInfo && supportedBundles.contains(profInfo->bundleVersion)
                   ? std::make_optional(profInfo->bundleVersion)
                   : std::nullopt;
    }
    auto resourceType = getResourceType();
    if (resourceType == Parameters::resourceTypeName)
    {
        const auto createTaskVer = parameterCreateTaskBundleVersion(jsonDocument());
        if (createTaskVer && supportedBundles.contains(*createTaskVer))
        {
            return createTaskVer;
        }
        const auto activateTaskVer = parameterActivateTaskBundleVersion(jsonDocument());
        if (activateTaskVer && supportedBundles.contains(*activateTaskVer))
        {
            return activateTaskVer;
        }

    }
    else if (resourceType == Bundle::resourceTypeName)
    {
        const auto* info = getProfileInfoFromBundle(jsonDocument());
        if (info)
        {
            return info->bundleVersion;
        }
    }
    return std::nullopt;
}


template<typename SchemaVersionType>
std::optional<SchemaVersionType> ResourceBase::getNonBundleSchemaVersion(std::string_view profileName) const
{
    if constexpr (! std::is_same_v<SchemaVersionType, ResourceVersion::NotProfiled>)
    {
        const auto* profInfo = model::ResourceVersion::profileInfoFromProfileName(profileName);

        if (profInfo && std::holds_alternative<SchemaVersionType>(profInfo->version))
        {
            return std::get<SchemaVersionType>(profInfo->version);
        }
    }
    return std::nullopt;
}


template<typename SchemaVersionType>
std::optional<SchemaVersionType> model::ResourceBase::getBundleSchemaVersion() const
{
    const auto* profInfo = getProfileInfoFromBundle(jsonDocument());
    return profInfo ? std::make_optional(get<SchemaVersionType>(profileVersionFromBundle(profInfo->bundleVersion)))
                    : std::nullopt;
}

namespace {
struct GetWorkflowOrPatientenRechnungProfile
{
    std::optional<ResourceVersion::WorkflowOrPatientenRechnungProfile> operator() (auto val)
        requires requires { ResourceVersion::WorkflowOrPatientenRechnungProfile(val); }
    {
        return std::make_optional(ResourceVersion::WorkflowOrPatientenRechnungProfile(val));
    }
    std::optional<ResourceVersion::WorkflowOrPatientenRechnungProfile> operator() (auto) { return std::nullopt; }
};
}

template<std::same_as<ResourceVersion::WorkflowOrPatientenRechnungProfile> SchemaVersionT>
std::optional<SchemaVersionT> model::ResourceBase::getSchemaVersion() const
{
    auto profileString = getProfileName();
    ModelExpect(profileString.has_value(), "Unable to determine profile for resource");
    const auto profileVersion = std::get<ResourceVersion::AnyProfileVersion>(
        ResourceVersion::profileVersionFromName(*profileString, ResourceVersion::allBundles()));
    return std::visit(GetWorkflowOrPatientenRechnungProfile{}, profileVersion);
}


template<typename SchemaVersionT>
std::optional<SchemaVersionT> model::ResourceBase::getSchemaVersion() const
{
    auto profileString = getProfileName();
    if (profileString)
    {
        return getNonBundleSchemaVersion<SchemaVersionT>(*profileString);
    }
    if (getResourceType() == model::Bundle::resourceTypeName)
    {
        return getBundleSchemaVersion<SchemaVersionT>();
    }
    return std::nullopt;
}


template<std::same_as<ResourceVersion::NotProfiled> SchemaVersionT>
std::optional<SchemaVersionT> model::ResourceBase::getSchemaVersion() const
{
    return std::nullopt;
}


UnspecifiedResource::UnspecifiedResource(NumberAsStringParserDocument&& document)
    : Resource<UnspecifiedResource>(std::move(document))
{
}

std::string_view ResourceBase::getResourceType() const
{
    static const rapidjson::Pointer resourceTypePointer(resource::ElementName::path(resource::elements::resourceType));
    return getStringValue(resourceTypePointer);
}

template std::optional<ResourceVersion::DeGematikErezeptWorkflowR4> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::DeGematikErezeptPatientenrechnungR4> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::KbvItaErp> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::AbgabedatenPkv> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::Fhir> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::WorkflowOrPatientenRechnungProfile> ResourceBase::getSchemaVersion() const;
template std::optional<ResourceVersion::NotProfiled> ResourceBase::getSchemaVersion() const;


template class Resource<AuditEvent>;
template class Resource<AuditMetaData>;
template class Resource<Binary>;
template class Resource<Bundle, ResourceVersion::NotProfiled>;
template class Resource<ChargeItem, ResourceVersion::DeGematikErezeptPatientenrechnungR4>;
template class Resource<Communication, ResourceVersion::WorkflowOrPatientenRechnungProfile>;
template class Resource<Composition>;
template class Resource<Consent, ResourceVersion::DeGematikErezeptPatientenrechnungR4>;
template class Resource<Device>;
template class Resource<Dosage, ResourceVersion::KbvItaErp>;
template class Resource<ErxReceipt>;
template class Resource<Extension>;
template class Resource<KbvBundle, ResourceVersion::KbvItaErp>;
template class Resource<KbvComposition, ResourceVersion::KbvItaErp>;
template class Resource<KbvCoverage, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationGeneric, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationCompounding, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationIngredient, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationPzn, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationRequest, ResourceVersion::KbvItaErp>;
template class Resource<KbvOrganization, ResourceVersion::KbvItaErp>;
template class Resource<KbvPractitioner, ResourceVersion::KbvItaErp>;
template class Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>;
template class Resource<MedicationDispense>;
template class Resource<MedicationDispenseBundle>;
template class Resource<MetaData, ResourceVersion::NotProfiled>;
template class Resource<OperationOutcome, ResourceVersion::Fhir>;
template class Resource<Parameters, ResourceVersion::NotProfiled>;
template class Resource<Patient, ResourceVersion::KbvItaErp>;
template class Resource<Reference>;
template class Resource<Signature>;
template class Resource<class Subscription, ResourceVersion::NotProfiled>;
template class Resource<Task>;
template class Resource<UnspecifiedResource>;
template class Resource<AbgabedatenPkvBundle, ResourceVersion::AbgabedatenPkv>;


}// namespace model
