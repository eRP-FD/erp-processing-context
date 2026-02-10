/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/TestUtils.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/EvdgaBundle.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/eu/GemErpEuPrMedication.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/AuditEvent.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/Device.hxx"
#include "shared/model/GemErpEuPrMedicationDispense.hxx"
#include "shared/model/GemErpEuPrOrganization.hxx"
#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseBundle.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/util/StaticData.hxx"

#include <date/tz.h>
#include <fhirtools/util/SaxHandler.hxx>
#include <algorithm>
#include <chrono>
#include <ranges>
#include <regex>

namespace testutils
{

namespace
{

void referenceTime(const std::shared_ptr<const fhirtools::Element>& resourceRoot, model::Timestamp& result)
{
    static const auto getAuthoredOn = Fhir::parseFhirPath("entry.resource.ofType(MedicationRequest).authoredOn");
    static const auto getWhenHandedOver = Fhir::parseFhirPath("whenHandedOver");
    ASSERT_TRUE(resourceRoot->isResource());
    auto resourceType = resourceRoot->resourceType();
    ASSERT_FALSE(resourceType.empty());
    const auto* backend = std::addressof(resourceRoot->getFhirStructureRepository());
    if (resourceType == "MedicationDispense")
    {
        auto whenHandedOverCtx = getWhenHandedOver->eval({backend->defaultView(), resourceRoot});
        auto maxWhenHandedOver = model::Timestamp::timepoint_t::min();
        for (const auto& whenHandedOver: whenHandedOverCtx.collection)
        {
            ASSERT_TRUE(whenHandedOver->hasValue());
            ASSERT_NO_THROW(maxWhenHandedOver = std::max(
                                maxWhenHandedOver,
                                model::Timestamp::fromGermanDate(whenHandedOver->asString()).toChronoTimePoint()))
                << whenHandedOver->asString();
        }
        ASSERT_GT(maxWhenHandedOver, std::chrono::system_clock::time_point::min());
        result = model::Timestamp(maxWhenHandedOver);
        return;
    }
    if (resourceType == "Bundle")
    {
        if (const auto& profiles = resourceRoot->profiles();
            profiles.size() == 1 && profiles[0].starts_with(model::resource::structure_definition::prescriptionItem))
        {
            auto authoredOnCtx = getAuthoredOn->eval({backend->defaultView(), resourceRoot});
            ASSERT_EQ(authoredOnCtx.collection.size(), 1);
            auto authoredOn = authoredOnCtx.collection[0];
            ASSERT_TRUE(authoredOn->hasValue());
            ASSERT_NO_THROW(result = model::Timestamp::fromFhirDateTime(authoredOn->asString()))
                << authoredOn->asString();
            return;
        }
    }
    result = model::Timestamp::now();
}

void additionalValidation(const std::shared_ptr<const ErpElement>& entry, const std::string& entryPath)
{
    model::NumberAsStringParserDocument doc;
    doc.CopyFrom(*entry->jsonValue(), doc.GetAllocator());
    auto resource = createResourceNoValidation(std::move(doc));
    ASSERT_NE(resource, nullptr);
    ASSERT_NO_FATAL_FAILURE(resource->additionalValidation()) << entryPath;
}

void validateBundleEntry(const std::shared_ptr<const ErpElement>& entry, const std::string& entryPath)
{
    const auto& fhirInstance = Fhir::instance();
    const auto& profiles = entry->profiles();
    auto reference = model::Timestamp::now();
    ASSERT_NO_FATAL_FAILURE(referenceTime(entry, reference));
    auto viewList = fhirInstance.structureRepository(reference);
    for (const auto& profile: profiles)
    {
        fhirtools::DefinitionKey key{profile};
        viewList = viewList.matchAll(key.url, value(key.version));
    }
    fhirtools::ValidatorOptions validatorOptions;
    std::optional<model::ProfileType> profileType;
    if (profiles.size() == 1)
    {
        profileType = model::findProfileType(profiles[0]);
    }
    if (profileType)
    {
        validatorOptions = fhirInstance.defaultValidatorOptions(*profileType, model::Timestamp::now());
    }
    // references have been checked already from top-level
    // must be disabled here, because references are unresolvable when the resource is validated alone.
    validatorOptions.levels.unresolveableReferenceInBundle = fhirtools::Severity::info;
    ASSERT_FALSE(viewList.empty()) << reference << " " << String::join(profiles);
    auto repoView = viewList.latest();
    const auto validationResult = fhirtools::FhirPathValidator::validate(repoView, entry, entryPath, validatorOptions);
    if (validationResult.highestSeverity() >= fhirtools::Severity::error)
    {
        validationResult.dumpToLog();
        FAIL();
    }
    if (profileType)
    {
        ASSERT_NO_FATAL_FAILURE(additionalValidation(entry, entryPath));
    }
}

void validateUnprofiledBundle(const model::FhirResourceBase& resource,
                              const fhirtools::ProfiledElementTypeInfo& elementId)
{
    static const auto getEntries = Fhir::parseFhirPath("Bundle.entry.resource");
    const auto& fhirInstance = Fhir::instance();
    const auto* backend = std::addressof(fhirInstance.backend());
    const auto& allGroups = backend->allGroups();
    const auto element = std::make_shared<ErpElement>(backend, std::weak_ptr<ErpElement>{}, elementId,
                                                      std::addressof(resource.jsonDocument()));
    const auto hl7FhirR4Core = fhirtools::FhirResourceViewGroupSet::create(
        "hl7.fhir.r4.core",
        {allGroups.at("hl7.fhir.r4.core-4.0.1-structures"), allGroups.at("hl7.fhir.r4.core-4.0.1-terminology")},
        backend);
    auto fhirOptions = fhirInstance.defaultValidatorOptions(model::ProfileType::fhir, model::Timestamp::now());
    // ignore meta.profile in all entries, beacuse each entry will be validated separately
    fhirOptions.validateMetaProfiles = false;
    fhirOptions.allowNonLiteralAuthorReference = true;
    // cannot be checked on base profiles:
    fhirOptions.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::disable;
    fhirOptions.levels.unknownMetaProfile = fhirtools::Severity::info;

    // there is no requirement, that we only produce Bundles, that have resolveable references
    // A_22122 even requires to not include the referenced resources (ChargeItem.supportingInformation)
    fhirOptions.levels.unresolveableReferenceInBundle = fhirtools::Severity::warning;

    // cannot be checked on base profiles:
    auto baseValidationResult = fhirtools::FhirPathValidator::validate(hl7FhirR4Core, element, "Bundle", fhirOptions);
    if (baseValidationResult.highestSeverity() >= fhirtools::Severity::error)
    {
        baseValidationResult.dumpToLog();
        FAIL() << resource.jsonDocument().serializeToJsonString();
    }
    auto entries = getEntries->eval({hl7FhirR4Core, element});
    for (size_t idx = 0; const auto& entryPtr : entries.collection)
    {
        auto entry = std::dynamic_pointer_cast<const ErpElement>(entryPtr);
        ASSERT_NE(entry, nullptr);
        validateBundleEntry(entry, "Bundle.entry[" + std::to_string(idx) + "].resource");
        ++idx;
    }
}
template<typename T>
std::unique_ptr<T> createResourceNoValidation(model::NumberAsStringParserDocument doc)
{
    return std::make_unique<T>(model::Resource<T>::fromJson(std::move(doc)));
}
}

std::unique_ptr<model::FhirResourceBase> createResourceNoValidation(model::NumberAsStringParserDocument doc)
{
    using namespace std::string_literals;
    namespace elements = model::resource::elements;
    static const rapidjson::Pointer metaProfilePtr{
        model::resource::ElementName::path(elements::meta, elements::profile)};
    static const rapidjson::Pointer resourceTypePtr{model::resource::ElementName::path(elements::resourceType)};
    const auto* metaProfile = metaProfilePtr.Get(doc);
    std::optional<model::ProfileType> profileType;
    if (metaProfile)
    {
        Expect(metaProfile->IsArray(), "meta.profile must be array.");
        for (const auto& profileUrl : metaProfile->GetArray())
        {
            profileType =
                model::findProfileType(model::NumberAsStringParserDocument::getStringValueFromValue(&profileUrl));
            if (profileType)
            {
                break;
            }
        }
    }
    if (! profileType)
    {

        const std::string_view resourceType = doc.getStringValueFromPointer(resourceTypePtr);
        TVLOG(0) << "resourceType: " << resourceType;
        if (resourceType == model::Bundle::resourceTypeName)
        {
            return createResourceNoValidation<model::Bundle>(std::move(doc));
        }
        if (resourceType == model::OperationOutcome::resourceTypeName)
        {
            return createResourceNoValidation<model::OperationOutcome>(std::move(doc));
        }
        if (resourceType == model::Binary::resourceTypeName)
        {
            return createResourceNoValidation<model::Binary>(std::move(doc));
        }
        return createResourceNoValidation<model::UnspecifiedResource>(std::move(doc));
    }

    switch (*profileType)
    {
        using enum model::ProfileType;
        case ActivateTaskParameters:
        case CreateTaskParameters:
            break;
        case GEM_ERP_PR_AuditEvent:
            return createResourceNoValidation<model::AuditEvent>(std::move(doc));
        case GEM_ERP_PR_Binary:
            return createResourceNoValidation<model::Binary>(std::move(doc));
        case fhir:// general FHIR schema
            break;
        case GEM_ERP_PR_Communication_DispReq:
        case GEM_ERP_PR_Communication_InfoReq:
        case GEM_ERPCHRG_PR_Communication_ChargChangeReq:
        case GEM_ERPCHRG_PR_Communication_ChargChangeReply:
        case GEM_ERP_PR_Communication_DiGA:
        case GEM_ERP_PR_Communication_Reply:
        case GEM_ERP_PR_Communication_Representative:
            return createResourceNoValidation<model::Communication>(std::move(doc));
        case GEM_ERP_PR_Composition:
            break;
        case GEM_ERP_PR_Device:
            return createResourceNoValidation<model::Device>(std::move(doc));
        case GEM_ERP_PR_Digest:
        case GEM_ERP_PR_Medication:
            break;
        case GEM_ERP_PR_MedicationDispense:
        case GEM_ERP_PR_MedicationDispense_DiGA:
            return createResourceNoValidation<model::MedicationDispense>(std::move(doc));
        case GEM_ERP_PR_PAR_CloseOperation_Input:
        case GEM_ERP_PR_PAR_DispenseOperation_Input:
            break;
        case KBV_PR_ERP_Bundle:
            return createResourceNoValidation<model::KbvBundle>(std::move(doc));
        case KBV_PR_ERP_Composition:
        case KBV_PR_ERP_Medication_Compounding:
        case KBV_PR_ERP_Medication_FreeText:
        case KBV_PR_ERP_Medication_Ingredient:
        case KBV_PR_ERP_Medication_PZN:
        case KBV_PR_ERP_PracticeSupply:
        case KBV_PR_ERP_Prescription:
            break;
        case KBV_PR_EVDGA_Bundle:
            return createResourceNoValidation<model::EvdgaBundle>(std::move(doc));
        case KBV_PR_EVDGA_HealthAppRequest:
        case KBV_PR_FOR_Coverage:
        case KBV_PR_FOR_Organization:
        case KBV_PR_FOR_Patient:
        case KBV_PR_FOR_Practitioner:
        case KBV_PR_FOR_PractitionerRole:
            break;
        case MedicationDispenseBundle:
            return createResourceNoValidation<model::MedicationDispenseBundle>(std::move(doc));
        case GEM_ERP_PR_Bundle:
        case GEM_ERP_PR_Task:
            return createResourceNoValidation<model::Task>(std::move(doc));
        case GEM_ERPCHRG_PR_ChargeItem:
            return createResourceNoValidation<model::ChargeItem>(std::move(doc));
        case GEM_ERPCHRG_PR_Consent:
        case GEM_ERPEU_PR_Consent:
            return createResourceNoValidation<model::Consent>(std::move(doc));
        case PatchChargeItemParameters:
        case GEM_ERPCHRG_PR_PAR_Patch_ChargeItem_Input:
            break;
        case DAV_PKV_PR_ERP_AbgabedatenBundle:
            return createResourceNoValidation<model::AbgabedatenPkvBundle>(std::move(doc));
        case Subscription:
            break;
        case OperationOutcome:
            return createResourceNoValidation<model::OperationOutcome>(std::move(doc));
        case EPAOpRxPrescriptionERPOutputParameters:
        case EPAOpRxDispensationERPOutputParameters:
        case ProvidePrescriptionErpOp:
        case CancelPrescriptionErpOp:
        case ProvideDispensationErpOp:
        case HealthcareServiceDirectory:
        case OrganizationDirectory:
        case LocationDirectory:
        case EPAMedicationPZNIngredient:
        case GEM_ERPEU_PR_PAR_Access_Authorization_Request:
        case GEM_ERPEU_PR_PAR_Access_Authorization_Response:
        case GEM_ERPEU_PR_PAR_PATCH_Task_Input:
        case GEM_ERPEU_PR_PAR_GET_Prescription_Input:
        case GEM_ERPEU_PR_PAR_CloseOperation_Input:
        case ERP_TPrescription_CarbonCopy:
        case ERP_TPrescription_Organization:
            break;
        case GEM_ERPEU_PR_MedicationDispense:
            return createResourceNoValidation<model::GemErpEuPrMedicationDispense>(std::move(doc));
        case GEM_ERPEU_PR_Medication:
            return createResourceNoValidation<model::GemErpEuPrMedication>(std::move(doc));
        case GEM_ERPEU_PR_Practitioner:
            return createResourceNoValidation<model::GemErpEuPrPractitioner>(std::move(doc));
        case GEM_ERPEU_PR_PractitionerRole:
            return createResourceNoValidation<model::GemErpEuPrPractitionerRole>(std::move(doc));
        case GEM_ERPEU_PR_Organization:
            return createResourceNoValidation<model::GemErpEuPrOrganization>(std::move(doc));
    }
    Fail2("resource creation not supported for: "s.append(magic_enum::enum_name(*profileType)), UnsupportedResourceTypeException);
}


std::set<fhirtools::ValidationError> validationResultFilter(const fhirtools::ValidationResults& validationResult,
                                                            const fhirtools::ValidatorOptions& options)
{
    fhirtools::ValidationResults allowList;
    // TODO ERP-12940 begin: remove allowlist entries:
    allowList.add(options.levels.unreferencedBundledResource,
                  "reference must be resolvable: Binary/PrescriptionDigest-.+",
                  "Bundle.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[1].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(
        options.levels.unreferencedBundledResource, "reference must be resolvable: Binary/PrescriptionDigest-.+",
        "Bundle.entry[3].resource{Bundle}.entry[0].resource{Composition}.section[0].entry[0]", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[1].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    allowList.add(options.levels.unreferencedBundledResource,
                  "Missing reference chain from Composition: https://.+/Binary/PrescriptionDigest-.+",
                  "Bundle.entry[3].resource{Bundle}.entry[2].resource{Binary}", nullptr);
    // TODO ERP-12940 end

    // allows description strings to partly match to be considered equal.
    auto customLess = [](const fhirtools::ValidationError& l, const fhirtools::ValidationError& r) {
        if (l.profile == r.profile && l.fieldName == r.fieldName && l.reason < r.reason)
        {
            if (l.reason.index() == r.reason.index() &&
                std::holds_alternative<fhirtools::ValidationError::MessageReason>(l.reason))
            {
                auto lr = std::get<fhirtools::ValidationError::MessageReason>(l.reason);
                auto rr = std::get<fhirtools::ValidationError::MessageReason>(r.reason);
                if (std::get<fhirtools::Severity>(lr) == std::get<fhirtools::Severity>(rr))
                {
                    auto lrr = std::get<std::string>(lr);
                    auto rrr = std::get<std::string>(rr);
                    // consider lr starts_with rr and vice versa as equals.
                    std::regex reg1(lrr);
                    std::regex reg2(rrr);
                    if (std::regex_match(rrr, reg1) || std::regex_match(lrr, reg2))
                    {
                        return false;
                    }
                }
            }
        }
        return l < r;
    };

    std::set<fhirtools::ValidationError> filteredValidationErrors;
    std::ranges::set_difference(validationResult.results(), allowList.results(),
                                std::inserter(filteredValidationErrors, filteredValidationErrors.begin()),
                                customLess);
    return filteredValidationErrors;
}

void validate(const model::FhirResourceBase& resource)
{
    const auto& fhirInstance = Fhir::instance();
    const auto& resourceType{resource.getResourceType()};
    fhirtools::ValidatorOptions options{.allowNonLiteralAuthorReference = true};
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> repoView;
    if (auto profileName = resource.getProfileName();
        profileName && profileName != model::resource::structure_definition::operationoutcome)
    {
        auto viewList = fhirInstance.structureRepository(model::Timestamp::now());
        fhirtools::DefinitionKey key{*profileName};
        ASSERT_TRUE(key.version.has_value()) << " missing version in profile: " << *profileName;
        repoView = viewList.match(key.url, *key.version);
        ASSERT_NE(repoView, nullptr) << " no view for profile: " << *profileName;
    }
    else if (resourceType == model::Bundle::resourceTypeName)
    {
        validateUnprofiledBundle(resource, fhirtools::ProfiledElementTypeInfo{fhirInstance.backend(), resourceType});
        return;
    }
    else
    {
        repoView = fhirInstance.structureRepository(model::Timestamp::now()).latest();
    }
    ASSERT_NE(repoView, nullptr);
    auto validationResult = resource.genericValidate(model::ProfileType::fhir, options, repoView);
    auto filteredValidationErrors = validationResultFilter(validationResult, options);
    for (const auto& item : filteredValidationErrors)
    {
        ASSERT_LT(item.severity(), fhirtools::Severity::error) << to_string(item);
    }
}

std::unique_ptr<model::FhirResourceBase> createResource(model::NumberAsStringParserDocument doc)
{
    auto resource = createResourceNoValidation(std::move(doc));
    if (resource)
    {
        validate(*resource);
    }
    return resource;
}

std::unique_ptr<model::FhirResourceBase> createResourceFromJson(std::string_view jsonStr)
{
    auto jsonValidator = StaticData::getJsonValidator();
    auto doc = model::NumberAsStringParserDocument::fromJson(jsonStr);
    jsonValidator->validate(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(doc), SchemaType::fhir);
    return createResource(std::move(doc));
}

std::unique_ptr<model::FhirResourceBase> createResourceFromXml(std::string_view xmlStr)
{
    auto xmlValidator = StaticData::getXmlValidator();
    auto fhirSchemaValidationContext = xmlValidator->getSchemaValidationContext(SchemaType::fhir);
    Expect3(fhirSchemaValidationContext != nullptr, "Failed to get Schema Validator for FHIR base schema.",
            std::logic_error);
    fhirtools::SaxHandler{}.validateStringView(xmlStr, *fhirSchemaValidationContext);
    return createResource(Fhir::instance().converter().xmlStringToJson(xmlStr));
}

std::unique_ptr<model::FhirResourceBase> createResource(std::string_view doc)
{
    auto indicator = doc.find_first_of("<{");
    if (indicator == std::string_view::npos)
    {
        return nullptr;
    }
    if (doc[indicator] == '<')
    {
        return createResourceFromXml(doc);
    }
    return createResourceFromJson(doc);
}

std::string shiftDate(const std::string& realDate)
{
    using namespace date::literals;
    static const char format[] = "%Y-%m-%d";
    const date::days globalOffset{Configuration::instance().getIntValue(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS)};

    std::istringstream realDateStream{realDate};
    date::sys_days realDateAsDate;
    date::from_stream(realDateStream, format, realDateAsDate);

    return date::format(format, realDateAsDate + globalOffset);
}

testutils::ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const AsConfiguredTag&)
{
    envGuards.emplace_back("ERP_FHIR_REFERENCE_TIME_OFFSET_DAYS", std::nullopt);
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const fhirtools::Date& startDate)
    : ShiftFhirResourceViewsGuard(calculateOffset(viewId, startDate))
{
}

std::chrono::days ShiftFhirResourceViewsGuard::calculateOffset(const std::string& viewId, const fhirtools::Date& startDate)
{
    std::list<gsl::not_null<std::shared_ptr<const fhirtools::FhirResourceViewConfiguration::ViewConfig>>> cleanViews;
    {
        // clear any Environment-Variables affecting validity so we get a cleanViews that has mStart and mEnd as configured
        ShiftFhirResourceViewsGuard clear{asConfigured};
        cleanViews = Fhir::instance().fhirResourceViewConfiguration().allViews();
    }
    date::local_days epoch{date::days::zero()};
    auto refView = std::ranges::find(cleanViews, viewId, &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
    Expect3(refView != cleanViews.end(), "no such view: " + viewId, std::logic_error);
    date::year_month_day refStart{(*refView)->mStart.value_or(epoch)};
    return date::sys_days{date::year_month_day{startDate}} - date::sys_days{refStart};
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::chrono::days& offset)
{
    envGuards.emplace_back("ERP_FHIR_REFERENCE_TIME_OFFSET_DAYS", std::to_string(offset.count()));
}

ShiftFhirResourceViewsGuard::ShiftFhirResourceViewsGuard(const std::string& viewId, const date::sys_days& startDate)
    : ShiftFhirResourceViewsGuard{viewId, fhirtools::Date{date::year_month_day{startDate}, fhirtools::Date::Precision::day}}
{
}

static bool enable166(model::PrescriptionType prescriptionType)
{
    if (ResourceTemplates::Versions::KBV_ERP_current() >= ResourceTemplates::Versions::KBV_ERP_1_4_0)
    {
        return true;
    }
    return prescriptionType != model::PrescriptionType::tRezept;
}
static bool canBeEu(model::PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            return true;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::tRezept:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::direkteZuweisungPkv:
            break;
    }
    return false;
}

std::vector<model::PrescriptionType> gkvPrescriptionTypes()
{
    std::vector<model::PrescriptionType> prescriptionTypes;
    for (auto prescriptionType : magic_enum::enum_values<model::PrescriptionType>() |
                                     std::ranges::views::filter(&model::canBeGkv) |
                                     std::ranges::views::filter(&enable166))
    {
        prescriptionTypes.emplace_back(prescriptionType);
    }
    return prescriptionTypes;
}
std::vector<model::PrescriptionType> pkvPrescriptionTypes()
{
    std::vector<model::PrescriptionType> prescriptionTypes;
    for (auto prescriptionType : magic_enum::enum_values<model::PrescriptionType>() |
                                     std::ranges::views::filter(&model::canBePkv) |
                                     std::ranges::views::filter(&enable166))
    {
        prescriptionTypes.emplace_back(prescriptionType);
    }
    return prescriptionTypes;
}
std::vector<model::PrescriptionType> euPrescriptionTypes()
{
    std::vector<model::PrescriptionType> prescriptionTypes;
    for (auto prescriptionType : magic_enum::enum_values<model::PrescriptionType>() |
                                     std::ranges::views::filter(&canBeEu) |
                                     std::ranges::views::filter(&enable166))
    {
        prescriptionTypes.emplace_back(prescriptionType);
    }
    return prescriptionTypes;
}
std::vector<model::PrescriptionType> nonEuPrescriptionTypes()
{
    std::vector<model::PrescriptionType> prescriptionTypes;
    for (auto prescriptionType :
         magic_enum::enum_values<model::PrescriptionType>() | std::ranges::views::filter([](auto t) {
             return ! canBeEu(t);
         }) | std::ranges::views::filter(&enable166))
    {
        prescriptionTypes.emplace_back(prescriptionType);
    }
    return prescriptionTypes;
}
std::vector<model::PrescriptionType> allPrescriptionTypes()
{
    std::vector<model::PrescriptionType> prescriptionTypes;
    for (auto prescriptionType :
         magic_enum::enum_values<model::PrescriptionType>() | std::ranges::views::filter(&enable166))
    {
        prescriptionTypes.emplace_back(prescriptionType);
    }
    return prescriptionTypes;
}

} // namespace testutils
