/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "shared/model/Extension.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/String.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/model/Resource.txx"

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

}// namespace

FhirResourceBase::FhirResourceBase(const Profile& profile)
    : ResourceBase()
{
    setProfile(profile);
}

FhirResourceBase::FhirResourceBase(const Profile& profile, const ::rapidjson::Document& resourceTemplate)
    : ResourceBase{resourceTemplate}
{
    setProfile(profile);
}

namespace
{
const rapidjson::Pointer resourceTypePointer(resource::ElementName::path(resource::elements::resourceType));
}

std::string_view FhirResourceBase::getResourceType() const
{
    return getStringValue(resourceTypePointer);
}

void FhirResourceBase::setResourceType(std::string_view resourceType)
{
    setValue(resourceTypePointer, resourceType);
}

void FhirResourceBase::setMetaProfile0(std::string_view metaProfile)
{
    static const rj::Pointer metaProfile0Ptr("/meta/profile/0");
    setValue(metaProfile0Ptr, metaProfile);
}

std::optional<model::Timestamp> model::FhirResourceBase::getValidationReferenceTimestamp() const
{
    return std::nullopt;
}

void FhirResourceBase::setProfile(const Profile& profile)
{
    std::visit(
        [this](const auto& p) {
            setProfile(p);
        },
        profile);
}

void FhirResourceBase::setProfile(const fhirtools::DefinitionKey& profileKey)
{
    Expect3(profileKey.version.has_value(), "Attempt to create resource without profile version: " + profileKey.url,
            std::logic_error);
    setValue(profilePointer, to_string(profileKey));
}

void FhirResourceBase::setProfile(NoProfileTag)
{
    // removeElement(profilePointer);
}

void FhirResourceBase::setProfile(ProfileType profileType)
{
    using namespace std::string_literals;
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const auto& profileUrl = profile(profileType);
    ModelExpect(profileUrl.has_value(),"Cannot set profile using profileType: "s += magic_enum::enum_name(profileType));
    auto supported = fhirInstance.structureRepository(model::Timestamp::now())
                         .supportedVersions(backend, {std::string{*profileUrl}});
    ModelExpect(!supported.empty(), "Cannot set profile - profileType not supported: "s += magic_enum::enum_name(profileType));
    setProfile(std::ranges::max(supported, {}, &fhirtools::DefinitionKey::version));
}

std::optional<UnspecifiedResource> FhirResourceBase::extractUnspecifiedResource(const rapidjson::Pointer& path) const
{
    const auto* entry = getValue(path);
    if (entry)
    {
        return UnspecifiedResource::fromJson(*entry);
    }
    return std::nullopt;
}

namespace
{
const rapidjson::Pointer idPointer{resource::ElementName::path(resource::elements::id)};
}
std::optional<std::string_view> FhirResourceBase::getId() const
{
    return getOptionalStringValue(idPointer);
}
void FhirResourceBase::setId(const std::string_view& id)
{
    setValue(idPointer, id);
}


std::optional<NumberAsStringParserDocument> FhirResourceBase::getExtensionDocument(const std::string_view& url) const
{
    static const rapidjson::Pointer extensionPtr(resource::ElementName::path(resource::elements::extension));
    static const rapidjson::Pointer urlPtr(resource::ElementName::path(resource::elements::url));
    const auto* extensionNode = findMemberInArray(extensionPtr, urlPtr, url);
    if (extensionNode == nullptr)
    {
        return std::nullopt;
    }
    std::optional<NumberAsStringParserDocument> extension{std::in_place};
    extension->CopyFrom(*extensionNode, extension->GetAllocator());
    return extension;
}

std::optional<std::string_view> FhirResourceBase::identifierUse() const
{
    static const rapidjson::Pointer identifierUsePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::use));
    return getOptionalStringValue(identifierUsePointer);
}

std::optional<std::string_view> FhirResourceBase::identifierTypeCodingCode() const
{
    static const rapidjson::Pointer identifierTypeCodingCodePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(identifierTypeCodingCodePointer);
}

std::optional<std::string_view> FhirResourceBase::identifierTypeCodingSystem() const
{
    static const rapidjson::Pointer identifierTypeCodingSystemPointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::system));
    return getOptionalStringValue(identifierTypeCodingSystemPointer);
}

std::optional<std::string_view> FhirResourceBase::identifierSystem() const
{
    static const rapidjson::Pointer identifierSystemPointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::system));
    return getOptionalStringValue(identifierSystemPointer);
}

std::optional<std::string_view> FhirResourceBase::identifierValue() const
{
    static const rapidjson::Pointer identifierValuePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::value));
    return getOptionalStringValue(identifierValuePointer);
}

ProfileType FhirResourceBase::getProfile() const
{
    const auto& profileString = getProfileName();
    ModelExpect(profileString.has_value(), "Cannot determine SchemaType without profile name");
    const auto& parts = String::split(*profileString, '|');
    const auto& profileWithoutVersion = parts[0];
    const auto& pathParts = String::split(profileWithoutVersion, '/');
    const auto& profile = pathParts.back();
    const auto& profileType = magic_enum::enum_cast<ProfileType>(profile);
    ModelExpect(profileType.has_value(),
                "Could not extract schema type from profile string: " + std::string(*profileString));
    return *profileType;
}

std::optional<std::string_view> FhirResourceBase::getProfileName() const
{
    static const rj::Pointer profileArrayPtr("/meta/profile");
    const auto* metaArray = getValue(profileArrayPtr);
    if (! (metaArray && metaArray->IsArray()))
    {
        return std::nullopt;
    }
    ModelExpect(metaArray->Size() == 1, "meta.profile array must have size 1");
    return getStringValue(profilePointer);
}

fhirtools::DefinitionKey FhirResourceBase::getDefinitionKeyFromMeta() const
{
    if (auto profileName = getProfileName())
    {
        return fhirtools::DefinitionKey{*profileName};
    }
    ModelFail("Could not get profileName from meta.profile");
}


gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>>
model::FhirResourceBase::getValidationView(ProfileType profileType) const
{
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const auto referenceTimestamp = getValidationReferenceTimestamp();
    ModelExpect(referenceTimestamp.has_value(), "missing reference timestamp.");
    const auto viewList = Fhir::instance().structureRepository(*referenceTimestamp);
    ModelExpect(! viewList.empty(), "Invalid reference timestamp: " + referenceTimestamp->toXsDateTime());
    std::shared_ptr<const fhirtools::FhirStructureRepository> repoView;
    const auto profileName = getProfileName();
    ModelExpect(profileName.has_value() || ! mandatoryMetaProfile(profileType), "missing mandatory meta.profile");
    if (viewList.size() == 1 || ! profileName)
    {
        // effectivly validates against base profiles, which are contained in any view.
        repoView = viewList.latest().view(backend);
    }
    else
    {
        fhirtools::DefinitionKey key{*profileName};
        repoView = viewList.match(std::addressof(fhirInstance.backend()), key);
        if (! repoView)
        {
            std::ostringstream supportedVersions;
            std::string_view sep{};
            for (const auto& supported :
                     viewList.supportedVersions(backend, {std::string{value(profile(profileType))}}))
            {
                supportedVersions << sep << to_string(supported);
                sep = ", ";
            }
            ModelExpect(repoView, "invalid profile " + std::string{*profileName} +
                                    " must be one of: " + std::move(supportedVersions).str());
        }
    }
    Expect3(repoView != nullptr, "no repo-view found.", std::logic_error);
    return repoView;
}

UnspecifiedResource::UnspecifiedResource(NumberAsStringParserDocument&& document)
: Resource<UnspecifiedResource>(std::move(document))
{
}

std::optional<model::Timestamp> UnspecifiedResource::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

// NOLINTBEGIN(bugprone-exception-escape, bugprone-forward-declaration-namespace)
template class Resource<OperationOutcome>;
template class Resource<UnspecifiedExtension>;
template class Resource<UnspecifiedResource>;

// NOLINTEND(bugprone-exception-escape, bugprone-forward-declaration-namespace)

}// namespace model
