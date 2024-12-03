/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Extension.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/ProfileType.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/String.hxx"


#ifdef _WIN32
#pragma warning(                                                                                                       \
    disable : 4127)// suppress warning C4127: Conditional expression is constant (rapidjson::Pointer::Stringify)
#endif

namespace model
{


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
    for (const auto& prof : fhirPathElement->profiles())
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
TDerivedModel Resource<TDerivedModel>::fromJson(const rapidjson::Value& json)
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

}// namespace model
