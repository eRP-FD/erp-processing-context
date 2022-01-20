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
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MetaData.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/model/OperationOutcome.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/model/Task.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/KbvMedicationModelHelper.hxx"
#include "erp/xml/SaxHandler.hxx"

#include <boost/format.hpp>
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
static const auto profilePointer = ::rapidjson::Pointer{"/meta/profile/0"};

rj::GenericStringRef<std::string_view::value_type> stringRefFrom(const std::string_view& s)
{
    return rj::StringRef(s.data(), s.size());
}
}

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

const NumberAsStringParserDocument& ResourceBase::jsonDocument() const
{
    return mJsonDocument;
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
    for (const auto& value : values) { array.PushBack(mJsonDocument.makeString(value), mJsonDocument.GetAllocator()); }

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
    return mJsonDocument.getOptionalStringValue(object, key);
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
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromXml(std::string_view xml, const XmlValidator& validator,
                                                                  const InCodeValidator& inCodeValidator,
                                                                  SchemaType schemaType)
{
    try
    {
        auto fhirSchemaValidationContext = validator.getSchemaValidationContextNoVer(SchemaType::fhir);
        FhirSaxHandler::validateXML(Fhir::instance().structureRepository(), xml, *fhirSchemaValidationContext);
        auto model = fromXmlNoValidation(xml);
        if (schemaType != SchemaType::fhir)
        {
            model.doValidation(xml, validator, inCodeValidator, schemaType);
        }
        return model;
    }
    catch (const ErpException&)
    {
        throw;
    }
    catch (const std::runtime_error& er)
    {
        TVLOG(1) << "runtime_error: " << er.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing / validation error", er.what());
    }
}

template<class TDerivedModel, typename SchemaVersionType>
void Resource<TDerivedModel, SchemaVersionType>::doValidation(std::string_view xml, const XmlValidator& validator,
                                                              const InCodeValidator& inCodeValidator,
                                                              SchemaType schemaType) const
{
    const auto schemaVersion = getSchemaVersion();

    auto schemaValidationContext = validator.getSchemaValidationContext(schemaType, schemaVersion);
    FhirSaxHandler::validateXML(Fhir::instance().structureRepository(), xml, *schemaValidationContext);

    inCodeValidator.validate(*this, schemaType, schemaVersion, validator);
}

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel Resource<TDerivedModel, SchemaVersionType>::fromJsonNoValidation(std::string_view json)
{
    return TDerivedModel(std::move(NumberAsStringParserDocument::fromJson(json)));
}

template<class TDerivedModel, typename SchemaVersionType>
TDerivedModel
Resource<TDerivedModel, SchemaVersionType>::fromJson(std::string_view json, const JsonValidator& jsonValidator,
                                                     const XmlValidator& xmlValidator,
                                                     const InCodeValidator& inCodeValidator, SchemaType schemaType)
{
    try
    {
        // do a basic validation using the fhir schema
        auto model = fromJsonNoValidation(json);
        jsonValidator.validate(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(model.jsonDocument()),
                               SchemaType::fhir);

        if (schemaType != SchemaType::fhir)
        {
            // Convert to XML and do XML validation
            const auto xml = Fhir::instance().converter().jsonToXmlString(model.jsonDocument());
            model.doValidation(xml, xmlValidator, inCodeValidator, schemaType);
        }
        return model;
    }
    catch (const ErpException&)
    {
        throw;
    }
    catch (const std::runtime_error& er)
    {
        TVLOG(1) << "runtime_error: " << er.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing / validation error", er.what());
    }
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

namespace
{
template<typename SchemaVersionType>
SchemaVersionType getSchemaVersionT(const std::string_view& versionString);

template<>
ResourceVersion::DeGematikErezeptWorkflowR4
getSchemaVersionT<ResourceVersion::DeGematikErezeptWorkflowR4>(const std::string_view& versionString)
{
    return ResourceVersion::str_vGematik(versionString);
}

template<>
ResourceVersion::KbvItaErp getSchemaVersionT<ResourceVersion::KbvItaErp>(const std::string_view& versionString)
{
    return ResourceVersion::str_vKbv(versionString);
}

template<typename SchemaVersionType>
SchemaVersionType getOldestVersion();

template<>
ResourceVersion::KbvItaErp getOldestVersion<ResourceVersion::KbvItaErp>()
{
    return ResourceVersion::KbvItaErp::v1_0_1;
}

template<>
ResourceVersion::DeGematikErezeptWorkflowR4 getOldestVersion<ResourceVersion::DeGematikErezeptWorkflowR4>()
{
    return ResourceVersion::DeGematikErezeptWorkflowR4::v1_0_3_1;
}
}

template<class TDerivedModel, typename SchemaVersionType>
SchemaVersionType Resource<TDerivedModel, SchemaVersionType>::getSchemaVersion() const
{
    if constexpr (!std::is_same_v<SchemaVersionType, ResourceVersion::NotProfiled>)
    {
        static const rj::Pointer profileArrayPtr("/meta/profile");
        static const rj::Pointer profilePtr("/meta/profile/0");
        const auto* metaArray = getValue(profileArrayPtr);
        ModelExpect(metaArray && metaArray->IsArray(), "/meta/profile array not found");
        ModelExpect(metaArray->Size() == 1, "/meta/profile array must have size 1");
        const auto profileString = getStringValue(profilePtr);
        const auto parts = String::split(profileString, '|');
        if (parts.size() == 2)
        {
            return getSchemaVersionT<SchemaVersionType>(parts[1]);
        }
        else
        {
            // assume oldest supported version.
            // TODO ERP-7953: remove this when unversionized profiles are no longer supported.
            return getOldestVersion<SchemaVersionType>();
        }
    }
    return {};
}

UnspecifiedResource::UnspecifiedResource(NumberAsStringParserDocument&& document)
    : Resource<UnspecifiedResource>(std::move(document))
{
}

template class Resource<AuditEvent>;
template class Resource<AuditMetaData>;
template class Resource<Binary>;
template class Resource<Bundle>;
template class Resource<class ChargeItem>;
template class Resource<Communication>;
template class Resource<Composition>;
template class Resource<class Consent>;
template class Resource<Device>;
template class Resource<Dosage, ResourceVersion::KbvItaErp>;
template class Resource<ErxReceipt>;
template class Resource<Extension>;
template class Resource<KbvBundle, ResourceVersion::KbvItaErp>;
template class Resource<KbvComposition, ResourceVersion::KbvItaErp>;
template class Resource<KbvCoverage, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationCompounding, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationIngredient, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationModelHelper, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationPzn, ResourceVersion::KbvItaErp>;
template class Resource<KbvMedicationRequest, ResourceVersion::KbvItaErp>;
template class Resource<KbvOrganization, ResourceVersion::KbvItaErp>;
template class Resource<KbvPractitioner, ResourceVersion::KbvItaErp>;
template class Resource<KbvPracticeSupply, ResourceVersion::KbvItaErp>;
template class Resource<MedicationDispense>;
template class Resource<MetaData>;
template class Resource<OperationOutcome>;
template class Resource<Parameters, ResourceVersion::NotProfiled>;
template class Resource<Patient, ResourceVersion::KbvItaErp>;
template class Resource<Reference>;
template class Resource<Signature>;
template class Resource<class Subscription>;
template class Resource<Task>;
template class Resource<UnspecifiedResource>;

}// namespace model
