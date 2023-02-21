#include "fhirtools/validator/internal/ReferenceContext.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <magic_enum.hpp>

using namespace fhirtools;

namespace
{
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
    void markContainedAnchors();
    void followReferences(AnchorType anchorType, ResourceInfo&);

    void checkMissingReference(const ResourceInfo& resource, const ValidatorOptions& options);
    void checkMissingResolution(ReferenceContext::ResourceInfo& resource, const ValidatorOptions& options);

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
    finalizer.markContainedAnchors();
    for (auto& res : refContext.mResources)
    {
        if (res->anchorType != AnchorType::none)
        {
            finalizer.followReferences(res->anchorType, *res);
        }
    }
    for (auto& res : refContext.mResources)
    {
        finalizer.checkMissingReference(*res, options);
        finalizer.checkMissingResolution(*res, options);
    }
    return finalizer.mResult;
}

void fhirtools::ReferenceContext::Finalizer::markContainedAnchors()
{
    for (auto& res : mContext.mResources)
    {
        for (const auto& target : res->referenceTargets)
        {
            for (const auto& contained : res->contained)
            {
                if (contained->identity == target.identity)
                {
                    contained->anchorType |= AnchorType::contained;
                }
            }
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void ReferenceContext::Finalizer::followReferences(AnchorType anchorType, ResourceInfo& info)
{
    anchorType = info.referencedByAnchor | info.anchorType | anchorType;
    if ((info.referencedByAnchor & anchorType) == anchorType)
    {
        return;
    }
    info.referencedByAnchor = anchorType;
    for (auto& ref : info.referenceTargets)
    {
        for (auto& resource : mContext.mResources)
        {
            if (resource->identity == ref.identity)
            {
                auto newAnchorType = resource->referencedByAnchor | anchorType | resource->anchorType;
                if (newAnchorType != resource->referencedByAnchor)
                {
                    followReferences(newAnchorType, *resource);
                }
            }
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

    const auto& repo = *resource.resourceRoot->getFhirStructureRepository();
    if (resource.resourceRoot->definitionPointer().profile()->isDerivedFrom(repo, constants::compositionUrl))
    {
        auto parent = resource.resourceRoot->parent();
        if (!parent || parent->definitionPointer().profile()->typeId() != "Bundle")
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

#ifndef NDEBUG
void fhirtools::ReferenceContext::dumpToLog() const
{
    for (const auto& res : mResources)
    {
        TVLOG(2) << res->elementFullPath << ": identity = " << res->identity;
        for (const auto& target : res->referenceTargets)
        {
            auto resolution = std::ranges::find(mResources, target.identity, &ResourceInfo::identity);
            auto resolutionMsg =
                resolution == mResources.end() ? " unresolved" : " resolved: " + (*resolution)->elementFullPath;
            TVLOG(3) << "    " << target.elementFullPath << ": references: " << target.identity << resolutionMsg;

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
}
#endif

std::ostream& fhirtools::operator<<(std::ostream& out, ReferenceContext::AnchorType anchorType)
{
    return out << magic_enum::enum_name(anchorType);
}

std::ostream& fhirtools::operator<<(std::ostream& out, const ReferenceContext::ResourceInfo& resInfo)
{
    out << resInfo.elementFullPath << ": "
        << R"({ "resourceFullUrl": ")" << resInfo.identity;
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
    out << R"(], "contained": [)";
    sep = {};
    for (const auto& contained : resInfo.contained)
    {
        out << sep << '"' << contained->identity << '"';
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
