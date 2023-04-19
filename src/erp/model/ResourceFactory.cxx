// (C) Copyright IBM Deutschland GmbH 2023
// (C) Copyright IBM Corp. 2023

#include "erp/fhir/internal/FhirSAXHandler.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"


namespace
{
std::list<std::string> supportedProfiles(SchemaType schemaType)
{
    std::list<std::string> result;
    if (schemaType != SchemaType::fhir)
    {
        for (const auto& profileBundle : model::ResourceVersion::supportedBundles())
        {
            auto profStr = model::ResourceVersion::profileStr(schemaType, profileBundle);
            if (profStr)
            {
                result.emplace_back(*profStr);
            }
        }
    }
    return result;
}

}


model::ResourceFactoryBase::ResourceFactoryBase(XmlDocCache xml)
    : mXmlDoc{std::move(xml)}
{
}


model::NumberAsStringParserDocument model::ResourceFactoryBase::fromJson(std::string_view jsonStr,
                                                                         const JsonValidator& validator)
{
    try
    {
        auto doc = NumberAsStringParserDocument::fromJson(jsonStr);
        validator.validate(NumberAsStringParserDocumentConverter::copyToOriginalFormat(doc), SchemaType::fhir);
        return doc;
    }
    catch (const ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Document is not a FHIR-JSON.", ex.what());
    }
}

model::NumberAsStringParserDocument model::ResourceFactoryBase::fromXml(std::string_view xmlStr,
                                                                        const XmlValidator& xmlValidator)
{
    try
    {
        auto fhirSchemaValidationContext = xmlValidator.getSchemaValidationContext(SchemaType::fhir);
        Expect3(fhirSchemaValidationContext != nullptr, "Failed to get Schema Validator for FHIR base schema.",
                std::logic_error);
        fhirtools::SaxHandler{}.validateStringView(xmlStr, *fhirSchemaValidationContext);
        return Fhir::instance().converter().xmlStringToJson(xmlStr);
    }
    catch (const ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Input is not a FHIR+XML document", ex.what());
    }
}


std::string_view model::ResourceFactoryBase::xmlDocument() const
{
    struct XmlDocument {
        std::string_view operator()(std::monostate) const
        {
            self.mXmlDoc.template emplace<std::string>(
                Fhir::instance().converter().jsonToXmlString(self.resource().jsonDocument()));
            return get<std::string>(self.mXmlDoc);
        }
        std::string_view operator()(std::string_view xml)
        {
            return xml;
        }
        const ResourceFactoryBase& self;
    };
    return std::visit(XmlDocument{*this}, mXmlDoc);
}

std::optional<model::ResourceVersion::FhirProfileBundleVersion> model::ResourceFactoryBase::fhirProfileBundleVersion(
    SchemaType schemaType, const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const
{
    auto fromContent = resource().fhirProfileBundleVersion(supportedBundles);
    if (fromContent)
    {
        return fromContent;
    }
    if (schemaType == SchemaType::fhir)
    {
        // fallback to an arbitrary supported profileBundle when validating SchemaType::fhir
        return *supportedBundles.begin();
    }
    else if (schemaType == SchemaType::PatchChargeItemParameters &&
             supportedBundles.contains(ResourceVersion::FhirProfileBundleVersion::v_2023_07_01))
    {
        // PatchChargeItemParameters is not profiled, but only valid on new profile bundle
        return ResourceVersion::FhirProfileBundleVersion::v_2023_07_01;
    }
    return std::nullopt;
}


void model::ResourceFactoryBase::validateInCode(SchemaType schemaType, const XmlValidator& xmlValidator,
                                                const InCodeValidator& inCodeValidator) const
{
    inCodeValidator.validate(resource(), schemaType, xmlValidator);
}

void model::ResourceFactoryBase::validateLegacyXSD(SchemaType schemaType, const XmlValidator& xmlValidator) const
{
    Expect3(schemaType != SchemaType::fhir, "redundant call to validateLegacyXSD using SchemaType::fhir",
            std::logic_error);
    auto schemaValidationContext = xmlValidator.getSchemaValidationContext(schemaType);
    Expect3(schemaValidationContext != nullptr, "failed to get schemaValidationContext", std::logic_error);
    fhirtools::SaxHandler{}.validateStringView(xmlDocument(), *schemaValidationContext);
}

fhirtools::ValidationResults model::ResourceFactoryBase::validateGeneric(const fhirtools::FhirStructureRepository& repo,
                                                                         const fhirtools::ValidatorOptions& options,
                                                                         const std::set<std::string>& profiles) const
{
    std::string resourceTypeName;
    resourceTypeName = resource().getResourceType();
    auto fhirPathElement = std::make_shared<ErpElement>(&repo, std::weak_ptr<const fhirtools::Element>{},
                                                        resourceTypeName, &resource().jsonDocument());
    std::ostringstream profileStr;
    std::string_view sep;
    for (const auto& prof : fhirPathElement->profiles())
    {
        profileStr << sep << prof;
        sep = ", ";
    }
    for (const auto& prof : profiles)
    {
        profileStr << sep << prof;
        sep = ", ";
    }

    auto timer = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryFhirValidation, "Generic FHIR Validation",
        {{"resourceType", resourceTypeName}, {std::string{"profiles"}, profileStr.str()}});

    return fhirtools::FhirPathValidator::validateWithProfiles(fhirPathElement, resourceTypeName, profiles, options);
}

std::optional<std::string_view> model::ResourceFactoryBase::getProfileName() const
{
    return resource().getProfileName();
}

std::string_view model::ResourceFactoryBase::getResourceType() const
{
    return resource().getResourceType();
}


template<typename SchemaVersionT>
model::ResourceFactorySchemaVersionBase<SchemaVersionT>::ResourceFactorySchemaVersionBase(XmlDocCache xmlDoc,
                                                                                          Options&& opt)
    : ResourceFactoryBase{xmlDoc}
    , mOptions{std::move(opt)}
{
}


template<typename SchemaVersionT>
std::optional<SchemaVersionT> model::ResourceFactorySchemaVersionBase<SchemaVersionT>::getSchemaVersion() const
{
    return resource().template getSchemaVersion<SchemaVersionT>();
}


template<typename SchemaVersionT>
void model::ResourceFactorySchemaVersionBase<SchemaVersionT>::genericValidationMode(
    Configuration::GenericValidationMode newMode)
{
    mOptions.genericValidationMode = newMode;
}

template<typename SchemaVersionT>
void model::ResourceFactorySchemaVersionBase<SchemaVersionT>::validatorOptions(fhirtools::ValidatorOptions newOpts)
{
    mOptions.validatorOptions = newOpts;
}


template<typename SchemaVersionT>
void model::ResourceFactorySchemaVersionBase<SchemaVersionT>::conditionalValidateGeneric(
    std::optional<ResourceVersion::FhirProfileBundleVersion> version, SchemaType schemaType,
    const fhirtools::ValidatorOptions& validatorOptions,
    const std::optional<ErpException>& validationErpException) const
{
    using enum Configuration::GenericValidationMode;
    Configuration::GenericValidationMode genericValidationMode{require_success};
    if (mOptions.genericValidationMode)
    {
        genericValidationMode = *mOptions.genericValidationMode;
    }
    else if (version.has_value())
    {
        genericValidationMode = Configuration::instance().genericValidationMode(*version);
    }
    switch (genericValidationMode)
    {
        case disable:
            if (validationErpException)
            {
                throw ExceptionWrapper<ErpException>::create(fileAndLine, *validationErpException);
            }
            return;
        case detail_only:
            if (! validationErpException)
            {
                return;
            }
            break;
        case ignore_errors:
        case require_success:
            break;
    }
    if (! version.has_value())
    {

        std::ostringstream msg;
        msg << "unsupported or unknown profile version";
        auto profs = supportedProfiles(schemaType);
        if (! profs.empty())
        {
            msg << R"( expected one of: [")" << String::join(profs, R"(", ")") << R"("])";
        }
        ModelFail(std::move(msg).str());
    }

    fhirtools::ValidationResults validationResult;
    if (validationErpException && validationErpException->diagnostics())
    {
        validationResult.add(fhirtools::Severity::error, *validationErpException->diagnostics(), {}, nullptr);
    }
    const auto& repo = Fhir::instance().structureRepository(*version);
    validationResult.merge(ResourceFactoryBase::validateGeneric(repo, validatorOptions, {}));
    auto highestSeverity = validationResult.highestSeverity();
#ifdef ENABLE_DEBUG_LOG
    if (highestSeverity >= fhirtools::Severity::error)
    {
        validationResult.dumpToLog();
    }
#endif
    if (validationErpException)
    {
        auto status = static_cast<std::underlying_type_t<HttpStatus>>(validationErpException->status()) > 400
                          ? validationErpException->status()
                          : HttpStatus::BadRequest;
        ErpFailWithDiagnostics(status, validationErpException->what(), validationResult.summary());
    }
    else if (highestSeverity >= fhirtools::Severity::error && genericValidationMode == require_success)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "FHIR-Validation error", validationResult.summary());
    }
}

template<typename SchemaVersionT>
void model::ResourceFactorySchemaVersionBase<SchemaVersionT>::validateEnforcedSchemaVersion() const
{
    using namespace std::string_literals;
    if (mOptions.enforcedVersion)
    {
        const auto schemaVersion = getSchemaVersion();
        ModelExpect(schemaVersion.has_value(),
                    "could not detect schema version but required "s.append(ResourceVersion::v_str(*schemaVersion)));
        ModelExpect(! mOptions.enforcedVersion || mOptions.enforcedVersion == schemaVersion,
                    "profile version mismatch for "s.append(resource().getProfileName().value_or("<unknown>"))
                        .append(", expected=")
                        .append(ResourceVersion::v_str(mOptions.enforcedVersion.value_or(SchemaVersionType{})))
                        .append(" found=")
                        .append(ResourceVersion::v_str(*schemaVersion)));
    }
}


template<typename SchemaVersionT>
void model::ResourceFactorySchemaVersionBase<SchemaVersionT>::validateNoAdditional(
    SchemaType schemaType, const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles) const
{
    Expect3(! supportedBundles.empty(), "supportedBundles must not be empty", std::logic_error);
    const auto& config = Configuration::instance();
    auto fhirProfBundleVer = ResourceFactoryBase::fhirProfileBundleVersion(schemaType, supportedBundles);
    auto validatorOptions = mOptions.validatorOptions.value_or(
        fhirProfBundleVer ? config.defaultValidatorOptions(*fhirProfBundleVer, schemaType)
                          : fhirtools::ValidatorOptions{});

    validateEnforcedSchemaVersion();

    if ((! fhirProfBundleVer || fhirProfBundleVer == ResourceVersion::FhirProfileBundleVersion::v_2022_01_01) &&
        schemaType != SchemaType::fhir)
    {
        try
        {
            validateLegacyXSD(schemaType, xmlValidator);
            validateInCode(schemaType, xmlValidator, inCodeValidator);
        }
        catch (const ErpException& erpException)
        {
            if (erpException.status() == HttpStatus::BadRequest)
            {
                conditionalValidateGeneric(fhirProfBundleVer, schemaType, validatorOptions, erpException);
            }
            throw;
        }
    }
    conditionalValidateGeneric(fhirProfBundleVer, schemaType, validatorOptions, std::nullopt);
}


template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::DeGematikErezeptWorkflowR4>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::DeGematikErezeptPatientenrechnungR4>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::KbvItaErp>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::AbgabedatenPkv>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::Fhir>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::WorkflowOrPatientenRechnungProfile>;
template class model::ResourceFactorySchemaVersionBase<model::ResourceVersion::NotProfiled>;
