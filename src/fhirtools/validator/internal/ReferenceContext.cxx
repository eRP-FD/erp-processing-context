// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "fhirtools/validator/internal/ReferenceContext.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "shared/util/Uuid.hxx"

#include <boost/algorithm/string/join.hpp>
#include <magic_enum/magic_enum.hpp>
#include <regex>

using namespace fhirtools;

namespace
{
constexpr auto unresolved = std::views::filter([](const ReferenceContext::ReferenceInfo& ref) {
    return !ref.identity.empty() && !ref.resolved;
});


bool isFhirRestfulUrl(const std::string& url)
{
    /// https://hl7.org/fhir/R4/references.html#regex
    static std::regex restfulRegex{
        R"--(((http|https):\/\/([A-Za-z0-9\-\\\.\:\%\$]*\/)+)?(Account|ActivityDefinition|AdverseEvent|AllergyIntolerance|Appointment|AppointmentResponse|AuditEvent|Basic|Binary|BiologicallyDerivedProduct|BodyStructure|Bundle|CapabilityStatement|CarePlan|CareTeam|CatalogEntry|ChargeItem|ChargeItemDefinition|Claim|ClaimResponse|ClinicalImpression|CodeSystem|Communication|CommunicationRequest|CompartmentDefinition|Composition|ConceptMap|Condition|Consent|Contract|Coverage|CoverageEligibilityRequest|CoverageEligibilityResponse|DetectedIssue|Device|DeviceDefinition|DeviceMetric|DeviceRequest|DeviceUseStatement|DiagnosticReport|DocumentManifest|DocumentReference|EffectEvidenceSynthesis|Encounter|Endpoint|EnrollmentRequest|EnrollmentResponse|EpisodeOfCare|EventDefinition|Evidence|EvidenceVariable|ExampleScenario|ExplanationOfBenefit|FamilyMemberHistory|Flag|Goal|GraphDefinition|Group|GuidanceResponse|HealthcareService|ImagingStudy|Immunization|ImmunizationEvaluation|ImmunizationRecommendation|ImplementationGuide|InsurancePlan|Invoice|Library|Linkage|List|Location|Measure|MeasureReport|Media|Medication|MedicationAdministration|MedicationDispense|MedicationKnowledge|MedicationRequest|MedicationStatement|MedicinalProduct|MedicinalProductAuthorization|MedicinalProductContraindication|MedicinalProductIndication|MedicinalProductIngredient|MedicinalProductInteraction|MedicinalProductManufactured|MedicinalProductPackaged|MedicinalProductPharmaceutical|MedicinalProductUndesirableEffect|MessageDefinition|MessageHeader|MolecularSequence|NamingSystem|NutritionOrder|Observation|ObservationDefinition|OperationDefinition|OperationOutcome|Organization|OrganizationAffiliation|Patient|PaymentNotice|PaymentReconciliation|Person|PlanDefinition|Practitioner|PractitionerRole|Procedure|Provenance|Questionnaire|QuestionnaireResponse|RelatedPerson|RequestGroup|ResearchDefinition|ResearchElementDefinition|ResearchStudy|ResearchSubject|RiskAssessment|RiskEvidenceSynthesis|Schedule|SearchParameter|ServiceRequest|Slot|Specimen|SpecimenDefinition|StructureDefinition|StructureMap|Subscription|Substance|SubstanceNucleicAcid|SubstancePolymer|SubstanceProtein|SubstanceReferenceInformation|SubstanceSourceMaterial|SubstanceSpecification|SupplyDelivery|SupplyRequest|Task|TerminologyCapabilities|TestReport|TestScript|ValueSet|VerificationResult|VisionPrescription)\/[A-Za-z0-9\-\.]{1,64}(\/_history\/[A-Za-z0-9\-\.]{1,64})?)--",
        std::regex_constants::optimize};
    return regex_match(url, restfulRegex);
}

[[maybe_unused]]
std::string resolutionMsg(const ReferenceContext::ReferenceInfo& target)
{
    if (target.identity.empty())
    {
        return " non-literal";
    }
    if (!target.resolved)
    {
        return " unresolved";
    }
    if (auto referencedResources = target.referencedResource.lock())
    {
        return " resolved: " + referencedResources->elementFullPath;
    }
    return " resolved: <out of validation scope>";
};

bool isBundle(const std::shared_ptr<const ReferenceContext::ResourceInfo>& r) {
    static constexpr std::string_view bundle{"Bundle"};
    return r && (r->resourceRoot->definitionPointer().profile()->typeId() == bundle);
};

}// anonymous namespace


class ReferenceContext::Finalizer
{
public:
    [[nodiscard]] static ValidationResults finalize(ReferenceContext& refContext, const ValidatorOptions&);

private:
    explicit Finalizer(ReferenceContext& context)
        : mContext(context)
    {
    }

    void followReferences(AnchorType anchorType, ResourceInfo&);

    void checkMissingReference(const ResourceInfo& resource, const ValidatorOptions& options);
    void checkMissingResolution(ReferenceContext::ResourceInfo& resource, const ValidatorOptions& options);
    void checkFullUrl(ReferenceContext::ResourceInfo& resource, const ValidatorOptions& options);
    void checkBundledResourceMissingId(ReferenceContext::ResourceInfo& resource, const ValidatorOptions& options);
    void checkUnresolveableReferenceInBundle(std::shared_ptr<ReferenceContext::ResourceInfo>& resource, const ValidatorOptions& options);

    ReferenceContext& mContext;
    ValidationResults mResult;
};

ValidationResults ReferenceContext::Finalizer::finalize(ReferenceContext& refContext,
                                                           const ValidatorOptions& options)

{
    if (!options.validateReferences)
    {
        return {};
    }
    Finalizer finalizer{refContext};
    for (auto& res : refContext.mResources)
    {
        finalizer.followReferences(res->referencedByAnchor | res->anchorType, *res);
    }
    for (auto& res : refContext.mResources)
    {
        finalizer.checkMissingReference(*res, options);
        finalizer.checkMissingResolution(*res, options);
        finalizer.checkFullUrl(*res, options);
        finalizer.checkBundledResourceMissingId(*res, options);
        finalizer.checkUnresolveableReferenceInBundle(res, options);
    }
    return finalizer.mResult;
}

// NOLINTNEXTLINE(misc-no-recursion)
void ReferenceContext::Finalizer::followReferences(AnchorType anchorType, ResourceInfo& info)
{
    info.referencedByAnchor = anchorType;
    for (auto& ref : info.referenceTargets)
    {
        if (ref.identity.empty())
        {
            continue;
        }
        auto referencedResource = ref.referencedResource.lock();
        if (!referencedResource)
        {
            auto [element, _] = ref.referencingElement->resolveReference(ref.identity, ref.elementFullPath);
            if (element == nullptr)
            {
                continue;
            }
            ref.resolved = true;
            auto resource = std::ranges::find_if(mContext.mResources, std::bind_front(std::equal_to{}, element), &ResourceInfo::resourceRoot);
            if (resource == mContext.mResources.end())
            {
                continue;
            }
            referencedResource = *resource;
            ref.referencedResource = *resource;
        }
        auto newAnchorType = referencedResource->referencedByAnchor | anchorType | referencedResource->anchorType;
        if (newAnchorType != referencedResource->referencedByAnchor)
        {
            followReferences(newAnchorType, *referencedResource);
        }
    }
}

void ReferenceContext::Finalizer::checkMissingReference(const ResourceInfo& resource, const ValidatorOptions& options)
{
    using std::max;
    auto missingReference = (~resource.referencedByAnchor & resource.referenceRequirement);
    switch (missingReference)
    {
        case AnchorType::all:
            mResult.add(max(options.levels.unreferencedBundledResource, options.levels.unreferencedContainedResource),
                        "Missing reference chain from Container and Composition: " + to_string(resource.identity),
                        resource.elementFullPath, nullptr);
            return;
        case AnchorType::composition:
            mResult.add(options.levels.unreferencedBundledResource,
                        "Missing reference chain from Composition: " + to_string(resource.identity),
                        resource.elementFullPath, nullptr);
            return;
        case AnchorType::contained:
            mResult.add(options.levels.unreferencedContainedResource,
                        "Missing reference chain from Container: " + to_string(resource.identity),
                        resource.elementFullPath, nullptr);
            return;
        case AnchorType::none:
            return;
    }
}

void ReferenceContext::Finalizer::checkMissingResolution(ResourceInfo& resource, const ValidatorOptions& options)
{
    static const std::set<std::string_view> mustResolve = {".subject",        ".encounter",     ".author",
                                                           ".attester.party", ".custodian",     ".event.detail",
                                                           ".section.author", ".section.focus", ".section.entry"};

    if (resource.resourceRoot->definitionPointer().profile()->isDerivedFrom(constants::compositionUrl))
    {
        if (!isBundle(resource.parent.lock()))
        {
            return;
        }
        for (auto& target : resource.referenceTargets)
        {
            if (mustResolve.contains(target.localPath))
            {
                auto hasTargetIdentity = [&](const auto& res) {
                    return res->identity == target.identity;
                };
                target.mustBeResolvable = true;
                if (target.identity.empty())
                {
                    if (options.allowNonLiteralAuthorReference && target.localPath == ".author")
                    {
                        continue;
                    }
                    std::ostringstream msg;
                    msg << "reference is not literal or invalid but must be resolvable: ";
                    target.referencingElement->json(msg);
                    mResult.add(options.levels.mandatoryResolvableReferenceFailure, msg.str(), target.elementFullPath,
                                nullptr);
                }
                else if (std::ranges::none_of(mContext.mResources, hasTargetIdentity))
                {
                    mResult.add(options.levels.unreferencedBundledResource,
                                "reference must be resolvable: " + to_string(target.identity), target.elementFullPath,
                                nullptr);
                }
            }
        }
    }
}

void fhirtools::ReferenceContext::Finalizer::checkFullUrl(ReferenceContext::ResourceInfo& resource,
                                                          const ValidatorOptions& options)
{
    if (!resource.fullUrl)
    {
        if (options.levels.bundleFullUrlMissing && isBundle(resource.parent.lock()))
        {
            mResult.add(*options.levels.bundleFullUrlMissing, ExtendedValidation::bundleFullUrlMissing,
                        resource.elementFullPath, nullptr);
        }
        return;
    }
    if ((! options.levels.bundleFullUrlIdMissmatch && ! options.levels.bundleFullUrlResourceTypeMissmatch &&
         ! options.levels.bundleFullUrlInvalidFormat))
    {
        return;
    }
    const std::string_view fullUrl{*resource.fullUrl};
    if (isFhirRestfulUrl(value(resource.fullUrl)))
    {
        auto slash = fullUrl.rfind('/');
        if (options.levels.bundleFullUrlIdMissmatch &&
            (resource.id && fullUrl.compare(slash + 1, std::string_view::npos, *resource.id) != 0))
        {
            mResult.add(*options.levels.bundleFullUrlIdMissmatch, ExtendedValidation::bundleFullUrlIdMissmatch,
                        resource.elementFullPath, nullptr);
        }
        if (!options.levels.bundleFullUrlResourceTypeMissmatch)
        {
            return;
        }
        size_t slash2 = fullUrl.rfind('/', std::max(slash, 1UL) - 1);
        if (fullUrl.compare(slash2 + 1, std::max(slash - slash2, 1UL) - 1, resource.resourceRoot->resourceType()) != 0)
        {
            mResult.add(*options.levels.bundleFullUrlResourceTypeMissmatch, ExtendedValidation::bundleFullUrlResourceTypeMissmatch,
                        resource.elementFullPath, nullptr);
        }
    }
    else if(!Uuid::isValidUrnUuid(fullUrl))
    {
        mResult.add(*options.levels.bundleFullUrlInvalidFormat, ExtendedValidation::bundleFullUrlInvalidFormat,
                    resource.elementFullPath, nullptr);
    }
}

void fhirtools::ReferenceContext::Finalizer::checkBundledResourceMissingId(ReferenceContext::ResourceInfo& resource,
                                                                           const ValidatorOptions& options)
{
    if (options.levels.bundledResourceMissingId && !resource.id)
    {
        if (auto parent = resource.parent.lock(); parent && isBundle(parent))
        {
            mResult.add(*options.levels.bundledResourceMissingId, ExtendedValidation::bundledResourceMissingId,
                        resource.elementFullPath, nullptr);
        }
    }
}

void fhirtools::ReferenceContext::Finalizer::checkUnresolveableReferenceInBundle(
    std::shared_ptr<ReferenceContext::ResourceInfo>& resource, const ValidatorOptions& options)
{
    if (!options.levels.unresolveableReferenceInBundle)
    {
        return;
    }
    if (!isBundle(resource))
    {
        return;
    }
    if (auto typeElements = resource->resourceRoot->subElements("type");
        !typeElements.empty() && typeElements[0]->hasValue() && typeElements[0]->asString() == "searchset")
    {
        // do not check references in Bundles of type searchset
        return;
    }
    std::set<std::shared_ptr<const ResourceInfo>> toCheck{resource};
    for (auto testee = toCheck.cbegin(); testee != toCheck.cend(); (toCheck.erase(testee), testee = toCheck.cbegin()))
    {
        std::ranges::copy_if((*testee)->children, std::inserter(toCheck, toCheck.begin()), std::not_fn(isBundle));
        for (const auto& ref: (*testee)->referenceTargets | unresolved)
        {
            mResult.add(value(options.levels.unresolveableReferenceInBundle),
                        ExtendedValidation::unresolveableReferenceInBundle, ref.elementFullPath, nullptr);
        }
    }
}


void fhirtools::ReferenceContext::merge(fhirtools::ReferenceContext&& source)
{
    mResources.splice(mResources.begin(), std::move(source.mResources));
}

void ReferenceContext::addResource(std::shared_ptr<ResourceInfo> newResource)
{
    mResources.emplace_back(std::move(newResource));
}

std::list<std::shared_ptr<ReferenceContext::ResourceInfo>>& ReferenceContext::resources()
{
    return mResources;
}

ValidationResults ReferenceContext::finalize(const ValidatorOptions& options)
{
    return Finalizer::finalize(*this, options);
}

void fhirtools::ReferenceContext::dumpToLog() const
{
#ifdef ENABLE_DEBUG_LOG
    for (const auto& res : mResources)
    {
        TVLOG(2) << *res;
        for (const auto& target : res->referenceTargets)
        {
            TVLOG(3) << "    " << target.elementFullPath << ": references: " << target.identity << resolutionMsg(target);
            for (const auto& profSet : target.targetProfileSets)
            {
                if (! profSet.second.empty())
                {
                    TVLOG(4) << "        from profile: " << profSet.first->url() << '|' << profSet.first->version();
                    for (const auto& targetProfile : profSet.second)
                    {
                        TVLOG(4) << "            targetProfile: " << targetProfile->url() << '|'
                                 << targetProfile->version();
                    }
                }
            }
        }
    }
#endif
}

std::ostream& fhirtools::operator<<(std::ostream& out, ReferenceContext::AnchorType anchorType)
{
    return out << magic_enum::enum_name(anchorType);
}

std::ostream& fhirtools::operator<<(std::ostream& out, const ReferenceContext::ResourceInfo& resInfo)
{
    out << resInfo.elementFullPath << ": "
        << R"({ "identity": ")" << resInfo.identity;
    if (resInfo.id)
    {
        out << R"(", "id": ")" << *resInfo.id;
    }
    if (resInfo.fullUrl)
    {
        out << R"(", "fullUrl": ")" << *resInfo.fullUrl;
    }
    out << R"(", "anchorType": ")" << resInfo.anchorType;
    out << R"(", "referenceRequirement": ")" << resInfo.referenceRequirement;
    out << R"(", "referencedByAnchor": ")" << resInfo.referencedByAnchor;
    out << R"(", "referenceTargets": [)";
    std::string_view sep;
    for (const auto& target : resInfo.referenceTargets)
    {
        out << sep << '"' << target.identity << '"';
        sep = ", ";
    }
    out << R"(], "children": [)";
    sep = {};
    for (const auto& children : resInfo.children)
    {
        out << sep << '"' << children->identity << '"';
        sep = ", ";
    }
    out << "]}";
    return out;
}

ReferenceContext::AnchorType fhirtools::operator|(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs)
{
    using UT = std::underlying_type_t<ReferenceContext::AnchorType>;
    return static_cast<ReferenceContext::AnchorType>(static_cast<UT>(lhs) | static_cast<UT>(rhs));
}

ReferenceContext::AnchorType& fhirtools::operator|=(ReferenceContext::AnchorType& lhs, ReferenceContext::AnchorType rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

ReferenceContext::AnchorType fhirtools::operator&(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs)
{
    using UT = std::underlying_type_t<ReferenceContext::AnchorType>;
    return static_cast<ReferenceContext::AnchorType>(static_cast<UT>(lhs) & static_cast<UT>(rhs));
}

ReferenceContext::AnchorType& fhirtools::operator&=(ReferenceContext::AnchorType& lhs, ReferenceContext::AnchorType rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

ReferenceContext::AnchorType fhirtools::operator^(ReferenceContext::AnchorType lhs, ReferenceContext::AnchorType rhs)
{
    using AnchorType = ReferenceContext::AnchorType;
    using UT = std::underlying_type_t<ReferenceContext::AnchorType>;
    return static_cast<ReferenceContext::AnchorType>(static_cast<UT>(lhs) ^ static_cast<UT>(rhs)) & AnchorType::all;
}

ReferenceContext::AnchorType fhirtools::operator~(ReferenceContext::AnchorType arg)
{
    using AnchorType = ReferenceContext::AnchorType;
    return AnchorType::all ^ arg;
}
