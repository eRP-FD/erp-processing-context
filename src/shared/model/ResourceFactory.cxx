// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "shared/validation/XmlValidator.hxx"


model::ResourceFactoryBase::ResourceFactoryBase(XmlDocCache xml, Options options)
    : mXmlDoc{std::move(xml)}
    , mOptions{options}
{
}


model::NumberAsStringParserDocument model::ResourceFactoryBase::fromJson(std::string_view jsonStr,
                                                                         const JsonValidator& validator)
{
    ErpExpect(!String::startsWithBom(jsonStr), HttpStatus::BadRequest, "illegal BOM before JSON resource");
    try
    {
        auto doc = NumberAsStringParserDocument::fromJson(jsonStr);
        validator.validate(NumberAsStringParserDocumentConverter::copyToOriginalFormat(doc), SchemaType::fhir);
        return doc;
    }
    catch (const ModelException& ex)
    {
        TVLOG(2) << jsonStr;
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Input is not a FHIR+JSON/UTF-8 document", ex.what());
    }
}

model::NumberAsStringParserDocument model::ResourceFactoryBase::fromXml(std::string_view xmlStr,
                                                                        const XmlValidator& xmlValidator)
{
    ErpExpect(!String::startsWithBom(xmlStr), HttpStatus::BadRequest, "illegal BOM before XML resource");
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
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Input is not a FHIR+XML/UTF-8 document", ex.what());
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


void model::ResourceFactoryBase::validate(
    ProfileType profileType, const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& repoView) const
{
    try
    {
        validateNoAdditional(profileType, repoView);
        if (options().additionalValidation)
        {
            resource().additionalValidation();
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

fhirtools::ValidationResults
model::ResourceFactoryBase::validateGeneric(const fhirtools::FhirStructureRepositoryView& repo,
                                            const fhirtools::ValidatorOptions& options,
                                            const std::set<fhirtools::DefinitionKey>& profiles) const
{
    std::string resourceTypeName;
    resourceTypeName = resource().getResourceType();
    rootElement = std::make_shared<ErpElement>(std::addressof(Fhir::instance().backend()),
                                               std::weak_ptr<const fhirtools::Element>{}, resourceTypeName,
                                               &resource().jsonDocument());

    auto timer = resource().timingLogTimer();
    return fhirtools::FhirPathValidator::validateWithProfiles(repo.shared_from_this(), rootElement, resourceTypeName,
                                                              profiles, options);
}

std::string model::ResourceFactoryBase::errorMessage(const fhirtools::ValidationResults& validationResults)
{
    for (const auto& result: validationResults.results())
    {
        if (result.severity() < fhirtools::Severity::error)
        {
            continue;
        }
        if (const auto* extendedValidationResult =
                std::get_if<fhirtools::ValidationError::ExtendedValidationFailure>(&result.reason))
        {
            switch (get<fhirtools::ExtendedValidation>(*extendedValidationResult))
            {
                using enum fhirtools::ExtendedValidation;
                case unslicedExtension:
                    A_22927_02.start("generate required message");
                    return "Unintendierte Verwendung von Extensions an unspezifizierter Stelle";
                    A_22927_02.finish();
                case invalidUrnUuidInUri:
                case bundleFullUrlMissing:
                case bundleFullUrlResourceTypeMissmatch:
                    break;
                case bundleFullUrlIdMissmatch:
                    A_26229_01.start("generate required message");
                    return "Die ID einer Ressource und die ID der zugehörigen fullUrl stimmen nicht überein.";
                    A_26229_01.finish();
                    break;
                case bundleFullUrlInvalidFormat:
                    A_26233_01.start("generate required message");
                    return "Format der fullUrl ist ungültig.";
                    A_26233_01.finish();
                    break;
                case bundledResourceMissingId:
                    A_27648.start("generate required message");
                    return "Die ID einer Ressource im Bundle ist nicht vorhanden";
                    A_27648.finish();
                    break;
                case unresolveableReferenceInBundle:
                    A_27649.start("generate required message");
                    return "Referenz einer Ressource konnte nicht aufgelöst werden.";
                    A_27649.finish();
                    break;
            }
        }
    }
    return "FHIR-Validation error";
}

std::optional<std::string_view> model::ResourceFactoryBase::getProfileName() const
{
    return resource().getProfileName();
}

model::ProfileType model::ResourceFactoryBase::getProfile() const
{
    return resource().getProfile();
}

std::string_view model::ResourceFactoryBase::getResourceType() const
{
    return resource().getResourceType();
}

std::optional<model::Timestamp> model::ResourceFactoryBase::getValidationReferenceTimestamp() const
{
    return resource().getValidationReferenceTimestamp();
}

gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepositoryView>>
model::ResourceFactoryBase::getValidationView() const
{
    return resource().getValidationView();
}

const model::ResourceFactoryBase::Options& model::ResourceFactoryBase::options() const
{
    return mOptions;
}

void model::ResourceFactoryBase::genericValidationMode(GenericValidationMode newMode)
{
    mOptions.genericValidationMode = newMode;
}

void model::ResourceFactoryBase::validatorOptions(fhirtools::ValidatorOptions newOpts)
{
    mOptions.validatorOptions = newOpts;
}

void model::ResourceFactoryBase::enableAdditionalValidation(bool enable)
{
    mOptions.additionalValidation = enable;
}

void model::ResourceFactoryBase::conditionalValidateGeneric(
    const std::set<fhirtools::DefinitionKey>& profiles,
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& repoView,
    const fhirtools::ValidatorOptions& validatorOptions,
    const std::optional<ErpException>& validationErpException) const
{
    auto genericValidationMode{GenericValidationMode::require_success};
    if (mOptions.genericValidationMode)
    {
        genericValidationMode = *mOptions.genericValidationMode;
    }
    switch (genericValidationMode)
    {
        using enum GenericValidationMode;
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
    fhirtools::ValidationResults validationResult;
    if (validationErpException && validationErpException->diagnostics())
    {
        validationResult.add(fhirtools::Severity::error, *validationErpException->diagnostics(), {}, nullptr);
    }
    const gsl::not_null useRepoView = repoView ? repoView : getValidationView().get();
    TVLOG(1) << "using view " << useRepoView->id() << " for " <<  getProfileName().value_or("UNKNOWN");
    validationResult.merge(ResourceFactoryBase::validateGeneric(*useRepoView, validatorOptions, profiles));
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
    else if (highestSeverity >= fhirtools::Severity::error &&
             genericValidationMode == GenericValidationMode::require_success)
    {
        TVLOG(2) << resource().serializeToJsonString();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, errorMessage(validationResult), validationResult.summary());
    }
}


void model::ResourceFactoryBase::validateNoAdditional(
    ProfileType profileType, const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& repoView) const
{
    using namespace std::string_literals;
    auto validatorOptions =
        (mOptions.validatorOptions)
            ? (*mOptions.validatorOptions)
            : (Fhir::instance().defaultValidatorOptions(profileType, value(getValidationReferenceTimestamp())));
    const auto prof = profile(profileType);
    Expect3(profileType == ProfileType::fhir || prof.has_value(),
            "Missing profile in view: "s.append(magic_enum::enum_name(profileType)), std::logic_error);
    std::set<fhirtools::DefinitionKey> profs;
    if (prof)
    {
        profs.emplace(*prof);
    }
    conditionalValidateGeneric(profs, repoView, validatorOptions, std::nullopt);
}
