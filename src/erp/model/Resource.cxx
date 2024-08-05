/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "erp/fhir/Fhir.hxx"
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
#include "erp/model/OperationOutcome.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/Subscription.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "erp/model/extensions/KBVEXERPAccident.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "erp/model/extensions/KBVMedicationCategory.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/String.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

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

} // namespace

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
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    gsl::not_null repoView = fhirInstance.structureRepository(model::Timestamp::now()).latest().view(backend);
    setProfile(value(profileWithVersion(profileType, *repoView)));
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
    ModelExpect(metaArray->Size() == 1, "/meta/profile array must have size 1");
    return getStringValue(profilePointer);
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
    if (viewList.size() == 1 || !mandatoryMetaProfile(profileType))
    {
        repoView = viewList.latest().view(backend);
    }
    else
    {
        auto profileName = getProfileName();
        ModelExpect(profileName.has_value(), "missing meta.profile");
        fhirtools::DefinitionKey key{*profileName};
        ModelExpect(key.version.has_value(), "missing version on meta.profile: " + to_string(key));
        repoView = viewList.match(std::addressof(fhirInstance.backend()), key.url, *key.version);
        if (! repoView)
        {
            std::ostringstream supportedVersions;
            std::string_view sep{};
            for (const auto& supported : viewList.supportedVersions(backend, {std::string{value(profile(profileType))}}))
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

// ---

template<class TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromXmlNoValidation(std::string_view xmlDocument)
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

template<class TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromXml(std::string_view xml, const XmlValidator& validator)
    requires FhirValidatable<TDerivedModel>
{
    using ResourceFactory = model::ResourceFactory<TDerivedModel>;
    auto resourceFactory = ResourceFactory::fromXml(xml, validator, {});
    ProfileType profileType = resourceFactory.profileType();
    resourceFactory.validatorOptions(Configuration::instance().defaultValidatorOptions(profileType));
    return std::move(resourceFactory).getValidated(profileType);
}

template<class TDerivedModel>
void Resource<TDerivedModel>::additionalValidation() const
{
}

template<class TDerivedModel>
fhirtools::ValidationResults Resource<TDerivedModel>::genericValidate(
    ProfileType profileType, const fhirtools::ValidatorOptions& options,
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& repoView) const
{
    std::string resourceTypeName;
    if constexpr (std::is_same_v<TDerivedModel, UnspecifiedResource>)
    {
        resourceTypeName = FhirResourceBase::getResourceType();
    }
    else
    {
        resourceTypeName = TDerivedModel::resourceTypeName;
    }
    auto fhirPathElement = std::make_shared<ErpElement>(repoView ? repoView : getValidationView().get(),
                                                        std::weak_ptr<const fhirtools::Element>{}, resourceTypeName,
                                                        &ResourceBase::jsonDocument());
    std::ostringstream profiles;
    std::string_view sep;
    std::set<fhirtools::DefinitionKey> profileKeys;
    if (const auto& modelProfile = profile(profileType))
    {
        profiles << sep << *modelProfile;
        sep = ", ";
        profileKeys.emplace(*modelProfile);
    }
    for (const auto& prof: fhirPathElement->profiles())
    {
        profileKeys.emplace(prof);
        profiles << sep << prof;
        sep = ", ";
    }
    auto timer = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryFhirValidation, "Generic FHIR Validation",
        {{"resourceType", resourceTypeName}, {std::string{"profiles"}, profiles.str()}});

    return fhirtools::FhirPathValidator::validateWithProfiles(fhirPathElement, resourceTypeName, profileKeys, options);
}

template<class TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromJsonNoValidation(std::string_view json)
{
    return TDerivedModel(std::move(NumberAsStringParserDocument::fromJson(json)));
}

template<class TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromJson(std::string_view json, const JsonValidator& jsonValidator)
    requires FhirValidatable<TDerivedModel>
{
    using ResourceFactory = model::ResourceFactory<TDerivedModel>;
    auto resourceFactory = ResourceFactory::fromJson(json, jsonValidator, {});
    ProfileType profileType = resourceFactory.profileType();
    resourceFactory.validatorOptions(Configuration::instance().defaultValidatorOptions(profileType));
    return std::move(resourceFactory).getValidated(profileType);
}

template<typename TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromJson(const rj::Value& json)
{
    NumberAsStringParserDocument document;
    document.CopyFrom(json, document.GetAllocator());
    return TDerivedModel(std::move(document));
}

template<typename TDerivedModel>
TDerivedModel Resource<TDerivedModel>::fromJson(model::NumberAsStringParserDocument&& json)
{
    return TDerivedModel(std::move(json));
}

UnspecifiedResource::UnspecifiedResource(NumberAsStringParserDocument&& document)
    : Resource<UnspecifiedResource>(std::move(document))
{
}

std::string_view FhirResourceBase::getResourceType() const
{
    static const rapidjson::Pointer resourceTypePointer(resource::ElementName::path(resource::elements::resourceType));
    return getStringValue(resourceTypePointer);
}

std::optional<model::Timestamp> model::FhirResourceBase::getValidationReferenceTimestamp() const
{
    return std::nullopt;
}

template<typename TDerivedModel>
gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>>
model::Resource<TDerivedModel>::getValidationView() const
{
    if constexpr (FhirValidatableProfileConstexpr<TDerivedModel>)
    {
        return getValidationView(TDerivedModel::profileType);
    }
    else if constexpr (FhirValidatableProfileFunction<TDerivedModel>)
    {
        return getValidationView(static_cast<const TDerivedModel*>(this)->profileType());
    }
    using namespace std::string_literals;
    ModelFail("Not a validateble resource: "s + typeid(TDerivedModel).name());
}

std::optional<model::Timestamp> UnspecifiedResource::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}


// NOLINTBEGIN(bugprone-exception-escape, bugprone-forward-declaration-namespace)
template class Resource<AbgabedatenPkvBundle>;
template class Resource<AuditEvent>;
template class Resource<AuditMetaData>;
template class Resource<Binary>;
template class Resource<Bundle>;
template class Resource<ChargeItem>;
template class Resource<ChargeItemMarkingFlags>;
template class Resource<ChargeItemMarkingFlag>;
template class Resource<Composition>;
template class Resource<Communication>;
template class Resource<Consent>;
template class Resource<Device>;
template class Resource<Dosage>;
template class Resource<ErxReceipt>;
template class Resource<KbvBundle>;
template class Resource<KbvComposition>;
template class Resource<KbvCoverage>;
template class Resource<KBVEXERPAccident>;
template class Resource<KBVEXFORLegalBasis>;
template class Resource<KBVMedicationCategory>;
template class Resource<KbvMedicationGeneric>;
template class Resource<KbvMedicationCompounding>;
template class Resource<KbvMedicationFreeText>;
template class Resource<KbvMedicationIngredient>;
template class Resource<KbvMedicationPzn>;
template class Resource<KbvMedicationRequest>;
template class Resource<KBVMultiplePrescription>;
template class Resource<KBVMultiplePrescription::Kennzeichen>;
template class Resource<KBVMultiplePrescription::Nummerierung>;
template class Resource<KBVMultiplePrescription::Zeitraum>;
template class Resource<KBVMultiplePrescription::ID>;
template class Resource<KbvOrganization>;
template class Resource<KbvPracticeSupply>;
template class Resource<KbvPractitioner>;
template class Resource<MedicationDispense>;
template class Resource<MedicationDispenseBundle>;
template class Resource<MetaData>;
template class Resource<OperationOutcome>;
template class Resource<CreateTaskParameters>;
template class Resource<ActivateTaskParameters>;
template class Resource<Patient>;
template class Resource<PatchChargeItemParameters>;
template class Resource<Reference>;
template class Resource<Signature>;
template class Resource<Subscription>;
template class Resource<Task>;
template class Resource<UnspecifiedExtension>;
template class Resource<UnspecifiedResource>;

// NOLINTEND(bugprone-exception-escape, bugprone-forward-declaration-namespace)

}// namespace model
