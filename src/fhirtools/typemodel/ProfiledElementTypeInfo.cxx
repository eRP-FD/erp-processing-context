/**
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 **/

#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"

#include <algorithm>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirSlicing.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/Constants.hxx"

using fhirtools::ProfiledElementTypeInfo;

ProfiledElementTypeInfo::ProfiledElementTypeInfo(const FhirStructureDefinition* profile)
    : ProfiledElementTypeInfo{profile , profile?profile->rootElement():nullptr}
{
}

ProfiledElementTypeInfo::ProfiledElementTypeInfo(const FhirStructureDefinition* profile,
                                             std::shared_ptr<const FhirElement> element)
    : mProfile(profile)
    , mElement(std::move(element))
{
    if (mProfile == nullptr)
    {
        FPFail2("profile is nullptr.", std::logic_error);
    }
    if (mElement == nullptr)
    {
        FPFail2("element is nullptr.", std::logic_error);
    }
}

ProfiledElementTypeInfo::ProfiledElementTypeInfo(const FhirStructureRepository* repo, std::string_view elementId)
    : ProfiledElementTypeInfo{get<ProfiledElementTypeInfo>(repo->resolveBaseContentReference(elementId))}
{
}

bool ProfiledElementTypeInfo::isResource() const
{
    FPExpect(mProfile->kind() != FhirStructureDefinition::Kind::slice, "Cannot determine isResource for kind slice");
    return mElement && mElement->isRoot() && mProfile->kind() == FhirStructureDefinition::Kind::resource;
}

std::vector<ProfiledElementTypeInfo> fhirtools::ProfiledElementTypeInfo::subElements() const
{
    std::vector<ProfiledElementTypeInfo> result;
    std::string prefix = element()->name() + '.';
    for (const auto& element : profile()->elements())
    {
        const auto& name = element->name();
        if (name.starts_with(prefix))
        {
            size_t dot = name.find('.', prefix.size());
            if (dot == std::string::npos)
            {
                result.emplace_back(mProfile, element);
            }
        }
    }
    return result;
}

std::optional<ProfiledElementTypeInfo> ProfiledElementTypeInfo::subField(const std::string_view& name) const
{
    std::string subElementId;
    subElementId.reserve(mElement->name().size() + name.size() + 1);
    subElementId.append(mElement->name()).append(1, '.').append(name);
    auto subElementDef = mProfile->findElement(subElementId);
    return subElementDef ? std::make_optional<ProfiledElementTypeInfo>(mProfile, std::move(subElementDef)) : std::nullopt;
}

std::list<ProfiledElementTypeInfo> ProfiledElementTypeInfo::subDefinitions(const FhirStructureRepository& repo,
                                                                       std::string_view name) const
{
    auto subElementDef = subField(name);
    if (! subElementDef)
    {
        return {};
    }
    std::list<ProfiledElementTypeInfo> result;
    result.emplace_back(*subElementDef);
    if (! subElementDef->element()->isBackbone())
    {
        auto typeId = subElementDef->element()->typeId();
        Expect3(typeId != "BackboneElement", "BackboneElement not marked backbone", std::logic_error);
        const FhirStructureDefinition* subType = nullptr;
        if (typeId.empty())
        {
            auto resolv = repo.resolveContentReference(*subElementDef->element());
            static_assert(std::is_reference_v<decltype(resolv.baseType)>);
            result.emplace_back(&resolv.baseType, resolv.element);
        }
        else
        {
            subType = repo.findTypeById(subElementDef->element()->typeId());
            FPExpect(subType != nullptr, "failed to resolve type: " + subElementDef->element()->typeId());
            result.emplace_back(subType);
        }
    }
    return result;
}
std::list<std::string> fhirtools::ProfiledElementTypeInfo::expandedNames(std::string_view name) const
{
    using namespace fhirtools::constants;
    std::list<std::string> result;
    if (subField(name))
    {
        result.emplace_back(name);
        return result;
    }

    const auto& originalName = mElement->originalName();
    std::string subOrigName;
    subOrigName.reserve(originalName.size() + 1 + name.size() + typePlaceholder.size());
    subOrigName.append(originalName).append(1, '.').append(name).append(typePlaceholder);
    for (const auto& element: mProfile->elements())
    {
        if (element->originalName() == subOrigName)
        {
            result.emplace_back(element->fieldName());
        }
    }
    return result;
}


void ProfiledElementTypeInfo::typecast(const FhirStructureRepository& repo, const FhirStructureDefinition* structDef)
{
    Expect3(structDef != nullptr, "defPtr must not be null", std::logic_error);
    Expect3(structDef->isDerivedFrom(repo, *profile()),
            "target type must be derived from source-type: " + to_string(*this) + " -> " + structDef->url() + '|' +
                structDef->version(),
            std::logic_error);
    Expect3(mElement->isRoot(), "typecast not supported for non-root: " + to_string(*this), std::logic_error);
    Expect3(structDef->derivation() != FhirStructureDefinition::Derivation::constraint,
            "typecast to constraint not allowed: " + to_string(*this) + " -> " + structDef->url() + '|' +
                structDef->version(),
            std::logic_error);
    mElement = FhirElement::Builder{*structDef->rootElement()}
                   .isArray(mElement->isArray())
                   .cardinalityMin(mElement->cardinality().min)
                   .cardinalityMax(mElement->cardinality().max)
                   .getAndReset();
    mProfile = structDef;
}


std::ostream& fhirtools::operator<<(std::ostream& out, const ProfiledElementTypeInfo& ptr)
{
    const auto* p = ptr.profile();
    const auto& e = ptr.element();
    out << p->url() << '|' << p->version() << '@' << e->originalName();
    return out;
}

std::string fhirtools::to_string(const ProfiledElementTypeInfo& ptr)
{
    std::ostringstream strm;
    strm << ptr;
    return strm.str();
}
