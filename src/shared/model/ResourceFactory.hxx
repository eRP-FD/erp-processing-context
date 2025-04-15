// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX

#include "shared/model/Resource.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <set>
#include <string_view>
#include <type_traits>
#include <variant>


class JsonValidator;
class XmlValidator;
class ErpElement;

namespace fhirtools
{
class FhirStructureRepository;
}

namespace model
{
template<typename ResourceT>
class ResourceFactory;
template <typename ResourceT>
ResourceFactory<ResourceT> for_resource(ResourceFactory<UnspecifiedResource>&& unspecifiedResource);

enum class GenericValidationMode
{
    disable,
    detail_only,
    ignore_errors,
    require_success
};

class ResourceFactoryBase
{
public:
    void validate(ProfileType profileType,
                  const std::shared_ptr<const fhirtools::FhirStructureRepository>& repoView = nullptr) const;

    [[nodiscard]] fhirtools::ValidationResults
    validateGeneric(const fhirtools::FhirStructureRepository&, const fhirtools::ValidatorOptions& opts,
                    const std::set<fhirtools::DefinitionKey>& profiles) const;
    std::optional<std::string_view> getProfileName() const;
    std::string_view getResourceType() const;
    std::optional<model::Timestamp> getValidationReferenceTimestamp() const;
    gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> getValidationView() const;

    struct Options {
        std::optional<GenericValidationMode> genericValidationMode = std::nullopt;
        std::optional<fhirtools::ValidatorOptions> validatorOptions = std::nullopt;
        bool additionalValidation = true;
    };

    const Options& options() const;
    void genericValidationMode(GenericValidationMode);
    void validatorOptions(fhirtools::ValidatorOptions);
    void enableAdditionalValidation(bool enable);


    virtual ~ResourceFactoryBase() noexcept = default;


protected:
    /// @brief runs generic validation depending on the configuration parameters
    void conditionalValidateGeneric(const std::set<fhirtools::DefinitionKey>& profiles,
                                    const std::shared_ptr<const fhirtools::FhirStructureRepository>& repoView,
                                    const fhirtools::ValidatorOptions& validatorOptions,
                                    const std::optional<ErpException>& validationErpException) const;
    void validateNoAdditional(ProfileType profileType,
                              const std::shared_ptr<const fhirtools::FhirStructureRepository>& view) const;

    using XmlDocCache = std::variant<std::monostate, std::string, std::string_view>;
    static NumberAsStringParserDocument fromXml(std::string_view xmlStr, const XmlValidator&);
    static NumberAsStringParserDocument fromJson(std::string_view jsonStr, const JsonValidator&);

    explicit ResourceFactoryBase(XmlDocCache xml, Options options);

    virtual const FhirResourceBase& resource() const = 0;

private:
    std::string_view xmlDocument() const;

    mutable XmlDocCache mXmlDoc;
    Options mOptions;
    mutable std::shared_ptr<ErpElement> rootElement;

    template <typename ResourceT>
    friend ResourceFactory<ResourceT> for_resource(ResourceFactory<UnspecifiedResource>&& unspecifiedResource);
};

template<typename ResourceT>
class ResourceFactory : public ResourceFactoryBase
{
    static_assert(std::is_base_of_v<ResourceBase, ResourceT>);

public:
    using ResourceType = ResourceT;

    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromXml(std::string_view, const XmlValidator& xmlValidator, Options = {});
    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromXml(std::string&&, const XmlValidator& xmlValidator, Options = {});

    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromJson(std::string_view, const JsonValidator& jsonValidator, Options = {});

    // assumes that jsonValue is already validated with fhir base schema
    static ResourceFactory fromJson(NumberAsStringParserDocument&& jsonValue, Options options = {});

    ResourceType getValidated(ProfileType profileType,
                              const std::shared_ptr<const fhirtools::FhirStructureRepository>& view = nullptr) &&;
    ResourceType getNoValidation() &&;

    ProfileType profileType() const
        requires FhirValidatableProfileFunction<ResourceT>;
    ProfileType profileType() const
        requires FhirValidatableProfileConstexpr<ResourceT>;

    ResourceFactory(const ResourceFactory&) = delete;
    ResourceFactory(ResourceFactory&&) noexcept = default;
    ResourceFactory& operator = (const ResourceFactory&) = delete;
    ResourceFactory& operator = (ResourceFactory&&) noexcept = default;

private:
    const ResourceType& resource() const override;
    explicit ResourceFactory(NumberAsStringParserDocument&&, XmlDocCache, Options opt);

    ResourceType mResource;

    friend ResourceFactory<ResourceT> for_resource<ResourceT>(ResourceFactory<UnspecifiedResource>&&);
};


///------------------------------------------------------

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromJson(std::string_view jsonDoc,
                                                                       const JsonValidator& jsonValidator, Options opt)
{
    return ResourceFactory<ResourceT>{ResourceFactoryBase::fromJson(jsonDoc, jsonValidator), std::monostate{}, opt};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromJson(NumberAsStringParserDocument&& jsonValue,
                                                                       Options options)
{
    return ResourceFactory<ResourceT>{std::move(jsonValue), std::monostate{}, options};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromXml(std::string_view xmlDoc,
                                                                      const XmlValidator& xmlValidator, Options opt)
{
    return ResourceFactory<ResourceT>{ResourceFactoryBase::fromXml(xmlDoc, xmlValidator), xmlDoc, opt};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromXml(std::string&& xmlDoc,
                                                                      const XmlValidator& xmlValidator, Options opt)
{
    auto jsonDoc = ResourceFactoryBase::fromXml(xmlDoc, xmlValidator);
    return ResourceFactory<ResourceT>{std::move(jsonDoc), std::move(xmlDoc), opt};
}

template<typename ResourceT>
ResourceFactory<ResourceT>::ResourceFactory(NumberAsStringParserDocument&& jsonDoc, XmlDocCache xmlDoc, Options opt)
    : ResourceFactoryBase{std::move(xmlDoc), opt}
    , mResource(ResourceType::fromJson(std::move(jsonDoc)))
{
}


template<typename ResourceT>
ResourceT ResourceFactory<ResourceT>::getNoValidation() &&
{
    return std::move(mResource);
}

template<typename ResourceT>
ProfileType model::ResourceFactory<ResourceT>::profileType() const
    requires FhirValidatableProfileFunction<ResourceT>
{
    return mResource.profileType();
}

template<typename ResourceT>
ProfileType model::ResourceFactory<ResourceT>::profileType() const
    requires FhirValidatableProfileConstexpr<ResourceT>
{
    return ResourceT::profileType;
}

template<typename ResourceT>
const ResourceT& ResourceFactory<ResourceT>::resource() const
{
    return mResource;
}


template<typename ResourceT>
ResourceT
ResourceFactory<ResourceT>::getValidated(ProfileType profileType,
                                         const std::shared_ptr<const fhirtools::FhirStructureRepository>& repoView) &&
{
    mResource.prepare();
    validate(profileType, repoView);
    return std::move(mResource);
}

template <typename ResourceT>
ResourceFactory<ResourceT> for_resource(ResourceFactory<UnspecifiedResource>&& unspecifiedResource)
{
    auto opts = unspecifiedResource.mOptions;
    auto xmlCache = std::move(unspecifiedResource.mXmlDoc);
    auto jsonDoc = std::move(unspecifiedResource).getNoValidation().jsonDocument();
    return ResourceFactory<ResourceT>{std::move(jsonDoc), std::move(xmlCache), opts};
}

}// namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX
