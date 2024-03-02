// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX

#include "erp/model/Resource.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/validation/SchemaType.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <string_view>
#include <type_traits>
#include <variant>


class JsonValidator;
class XmlValidator;
class InCodeValidator;

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

class ResourceFactoryBase
{
public:
    [[nodiscard]] fhirtools::ValidationResults validateGeneric(const fhirtools::FhirStructureRepository&,
                                                               const fhirtools::ValidatorOptions& opts,
                                                               const std::set<std::string>& profiles) const;

    void validateInCode(SchemaType, const XmlValidator&, const InCodeValidator&) const;
    void validateLegacyXSD(SchemaType, const XmlValidator&) const;
    std::optional<std::string_view> getProfileName() const;
    std::string_view getResourceType() const;
    std::optional<model::Timestamp> getValidationReferenceTimestamp() const;

    virtual ~ResourceFactoryBase() noexcept = default;


protected:
    using XmlDocCache = std::variant<std::monostate, std::string, std::string_view>;
    static NumberAsStringParserDocument fromXml(std::string_view xmlStr, const XmlValidator&);
    static NumberAsStringParserDocument fromJson(std::string_view jsonStr, const JsonValidator&);

    explicit ResourceFactoryBase(XmlDocCache xml);

    virtual const ResourceBase& resource() const = 0;
    std::optional<model::ResourceVersion::FhirProfileBundleVersion>
    fhirProfileBundleVersion(SchemaType schemaType,
                             const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const;

private:
    std::string_view xmlDocument() const;
    mutable XmlDocCache mXmlDoc;

    template <typename ResourceT>
    friend ResourceFactory<ResourceT> for_resource(ResourceFactory<UnspecifiedResource>&& unspecifiedResource);
};


template<typename SchemaVersionT>
class ResourceFactorySchemaVersionBase : public ResourceFactoryBase
{
public:
    ResourceFactorySchemaVersionBase() = default;
    ResourceFactorySchemaVersionBase(const ResourceFactorySchemaVersionBase&) = default;
    ResourceFactorySchemaVersionBase(ResourceFactorySchemaVersionBase&&) noexcept = default;

    using SchemaVersionType = SchemaVersionT;
    struct Options {
        std::optional<SchemaVersionType> fallbackVersion = std::nullopt;
        std::optional<Configuration::GenericValidationMode> genericValidationMode = std::nullopt;
        std::optional<SchemaVersionType> enforcedVersion = std::nullopt;
        std::optional<fhirtools::ValidatorOptions> validatorOptions;
        bool additionalValidation = true;
    };

    [[nodiscard]] std::optional<SchemaVersionT> getSchemaVersion() const;

    void genericValidationMode(Configuration::GenericValidationMode);
    void validatorOptions(fhirtools::ValidatorOptions);
    void enableAdditionalValidation(bool enable);

protected:
    explicit ResourceFactorySchemaVersionBase(XmlDocCache, Options&&);


    /// @brief runs generic validation depending on the configuration parameters
    /// @param version profile bundle version only needed, when validaton is actually performed
    /// @param validatorOptions options passed to validator
    /// @param validationErpException exception from previous validation, if any
    void conditionalValidateGeneric(std::optional<ResourceVersion::FhirProfileBundleVersion> version,
                                    const std::vector<std::string>& supportedProfiles,
                                    const fhirtools::ValidatorOptions& validatorOptions,
                                    const std::optional<ErpException>& validationErpException) const;
    void validateNoAdditional(SchemaType schemaType, const XmlValidator& xmlValidator,
                              const InCodeValidator& inCodeValidator,
                              const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const;
    void validateEnforcedSchemaVersion() const;

    Options mOptions;

    template <typename ResourceT>
    friend ResourceFactory<ResourceT> for_resource(ResourceFactory<UnspecifiedResource>&& unspecifiedResource);
};


template<typename ResourceT>
class ResourceFactory : public ResourceFactorySchemaVersionBase<typename ResourceT::SchemaVersionType>
{
    static_assert(std::is_base_of_v<ResourceBase, ResourceT>);

public:
    using ResourceType = ResourceT;
    using SchemaVersionType = typename ResourceT::SchemaVersionType;
    using SchemaVersionBase = ResourceFactorySchemaVersionBase<SchemaVersionType>;
    using Options = typename SchemaVersionBase::Options;
    using XmlDocCache = typename SchemaVersionBase::XmlDocCache;


    void validate(SchemaType schemaType, const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
                  const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const;

    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromXml(std::string_view, const XmlValidator& xmlValidator, Options = {});
    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromXml(std::string&&, const XmlValidator& xmlValidator, Options = {});

    // throws when the input doesn't conform to fhir base schema
    static ResourceFactory fromJson(std::string_view, const JsonValidator& jsonValidator, Options = {});

    // assumes that jsonValue is already validated with fhir base schema
    static ResourceFactory fromJson(NumberAsStringParserDocument&& jsonValue, Options options = {});

    ResourceType getValidated(SchemaType, const XmlValidator&, const InCodeValidator& inCodeValidator,
                              const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles =
                                  model::ResourceVersion::supportedBundles()) &&;
    ResourceType getNoValidation() &&;

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
    return ResourceFactory<ResourceT>{ResourceFactoryBase::fromJson(jsonDoc, jsonValidator), std::monostate{},
                                      std::move(opt)};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromJson(NumberAsStringParserDocument&& jsonValue,
                                                                       Options options)
{
    return ResourceFactory<ResourceT>{std::move(jsonValue), std::monostate{}, std::move(options)};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromXml(std::string_view xmlDoc,
                                                                      const XmlValidator& xmlValidator, Options opt)
{
    return ResourceFactory<ResourceT>{ResourceFactoryBase::fromXml(xmlDoc, xmlValidator), xmlDoc, std::move(opt)};
}

template<typename ResourceT>
ResourceFactory<ResourceT> model::ResourceFactory<ResourceT>::fromXml(std::string&& xmlDoc,
                                                                      const XmlValidator& xmlValidator, Options opt)
{
    auto jsonDoc = ResourceFactoryBase::fromXml(xmlDoc, xmlValidator);
    return ResourceFactory<ResourceT>{std::move(jsonDoc), std::move(xmlDoc), std::move(opt)};
}

template<typename ResourceT>
ResourceFactory<ResourceT>::ResourceFactory(NumberAsStringParserDocument&& jsonDoc, XmlDocCache xmlDoc, Options opt)
    : ResourceFactorySchemaVersionBase<SchemaVersionType>{xmlDoc, std::move(opt)}
    , mResource(ResourceType::fromJson(std::move(jsonDoc)))
{
}


template<typename ResourceT>
ResourceT ResourceFactory<ResourceT>::getNoValidation() &&
{
    return std::move(mResource);
}

template<typename ResourceT>
const ResourceT& ResourceFactory<ResourceT>::resource() const
{
    return mResource;
}

template<typename ResourceT>
void ResourceFactory<ResourceT>::validate(
    SchemaType schemaType, const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const
{
    try
    {
        SchemaVersionBase::validateNoAdditional(schemaType, xmlValidator, inCodeValidator, supportedBundles);
        if (SchemaVersionBase::mOptions.additionalValidation)
        {
            mResource.additionalValidation();
        }
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


template<typename ResourceT>
ResourceT ResourceFactory<ResourceT>::getValidated(
    SchemaType schemaType, const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) &&
{

    validate(schemaType, xmlValidator, inCodeValidator, supportedBundles);
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


extern template class ResourceFactorySchemaVersionBase<ResourceVersion::DeGematikErezeptWorkflowR4>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::DeGematikErezeptPatientenrechnungR4>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::KbvItaErp>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::AbgabedatenPkv>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::Fhir>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::WorkflowOrPatientenRechnungProfile>;
extern template class ResourceFactorySchemaVersionBase<ResourceVersion::NotProfiled>;

}// namespace model

#endif// ERP_PROCESSING_CONTEXT_MODEL_RESOURCEFACTORY_HXX
