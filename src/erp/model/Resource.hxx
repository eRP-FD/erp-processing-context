/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RESOURCE_HXX
#define ERP_PROCESSING_CONTEXT_RESOURCE_HXX

#include "erp/model/ProfileType.hxx"
#include "erp/model/ResourceBase.hxx"
#include "erp/model/Timestamp.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"

#include <variant>


namespace fhirtools
{
class ValidatorOptions;
}

namespace model
{
class UnspecifiedExtension;

template<typename T>
concept FhirValidatableProfileFunction = requires {
                                             {
                                                 std::declval<T>().profileType()
                                                 } -> std::convertible_to<model::ProfileType>;
                                         };

template<typename T>
concept FhirValidatableProfileConstexpr = requires {
                                              {
                                                  T::profileType
                                                  } -> std::convertible_to<model::ProfileType>;
                                          };

template<typename T>
concept FhirValidatable = FhirValidatableProfileFunction<T> || FhirValidatableProfileConstexpr<T>;

// NOLINTNEXTLINE(bugprone-exception-escape)
class FhirResourceBase : public ResourceBase
{
public:
    struct NoProfileTag {
    };
    static constexpr auto NoProfile = NoProfileTag{};
    using Profile = ::std::variant<fhirtools::DefinitionKey, ProfileType, NoProfileTag>;

    [[nodiscard]] std::string_view getResourceType() const;

    virtual std::optional<model::Timestamp> getValidationReferenceTimestamp() const;
    virtual gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> getValidationView() const = 0;
    virtual void prepare() {};
    virtual void additionalValidation() const = 0;

    [[nodiscard]] std::optional<std::string_view> identifierUse() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingCode() const;
    [[nodiscard]] std::optional<std::string_view> identifierTypeCodingSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierSystem() const;
    [[nodiscard]] std::optional<std::string_view> identifierValue() const;

    [[nodiscard]] ProfileType getProfile() const;
    [[nodiscard]] std::optional<std::string_view> getProfileName() const;

    std::optional<NumberAsStringParserDocument> getExtensionDocument(const std::string_view& url) const;

    template<typename ExtensionT = UnspecifiedExtension>
    std::optional<ExtensionT> getExtension(const std::string_view& url = ExtensionT::url) const;

    std::optional<UnspecifiedResource> extractUnspecifiedResource(const rapidjson::Pointer& path) const;

protected:
    using ResourceBase::ResourceBase;

    explicit FhirResourceBase(const Profile& profile);
    FhirResourceBase(const Profile& profile, const ::rapidjson::Document& resourceTemplate);

    void setProfile(NoProfileTag);
    void setProfile(const fhirtools::DefinitionKey& key);
    void setProfile(ProfileType profileType);
    void setProfile(const Profile& profile);

    gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>>
    getValidationView(ProfileType profileType) const;
};

template<class TDerivedModel>
// NOLINTNEXTLINE(bugprone-exception-escape)
class Resource : public FhirResourceBase
{
public:
    [[nodiscard]] static TDerivedModel fromXmlNoValidation(std::string_view xml);

    [[nodiscard]] static TDerivedModel fromXml(std::string_view xml, const XmlValidator& validator)
        requires FhirValidatable<TDerivedModel>;
    [[nodiscard]] static TDerivedModel fromJsonNoValidation(std::string_view json);

    [[nodiscard]] static TDerivedModel fromJson(std::string_view json, const JsonValidator& jsonValidator)
        requires FhirValidatable<TDerivedModel>;
    [[nodiscard]] static TDerivedModel fromJson(const rapidjson::Value& json);
    [[nodiscard]] static TDerivedModel fromJson(model::NumberAsStringParserDocument&& json);

    [[nodiscard]] fhirtools::ValidationResults
    genericValidate(ProfileType profileType, const fhirtools::ValidatorOptions& options,
                    const std::shared_ptr<const fhirtools::FhirStructureRepository>& repoView = nullptr) const;

    gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>> getValidationView() const override;

    void additionalValidation() const override;

protected:
    using FhirResourceBase::FhirResourceBase;
    using FhirResourceBase::getValidationView;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class UnspecifiedResource : public Resource<UnspecifiedResource>
{
protected:
    using Resource::Resource;

private:
    friend Resource<UnspecifiedResource>;
    explicit UnspecifiedResource(NumberAsStringParserDocument&& document);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
};

template<typename T>
void extensionCallable(const Extension<T>*);


template<typename T>

static consteval bool isExtension(const T* e)
    requires requires { extensionCallable(e); }
{
    return true;
}

template<typename T>
static consteval bool isExtension(const T*)
{
    return false;
}

template<typename ExtensionT>
std::optional<ExtensionT> FhirResourceBase::getExtension(const std::string_view& url) const
{
    static_assert(isExtension(static_cast<const ExtensionT*>(nullptr)),
                  "ExtensionT must be derived from model::Extension");
    auto extensionNode = getExtensionDocument(url);
    if (extensionNode)
    {
        return ExtensionT::fromJson(std::move(*extensionNode));
    }
    return std::nullopt;
}


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<class UnspecifiedResource>;


}// namespace model

#endif//ERP_PROCESSING_CONTEXT_RESOURCE_HXX
