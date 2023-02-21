/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/model/Element.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Collection.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <regex>
#include <string_view>
#include <utility>
#include <variant>

using fhirtools::Element;
using fhirtools::PrimitiveElement;
using fhirtools::ValidationResults;

namespace
{
decltype(auto) classicCtype()
{
    static decltype(auto) ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    return ctype;
}

bool isScheme(std::string_view str)
{
    using namespace std::string_view_literals;
    if (str.empty() || ! classicCtype().is(std::ctype_base::alpha, str[0]))
    {
        return false;
    }
    const char* notSchemeChar = std::ranges::find_if_not(str, [](const char c) {
        return classicCtype().is(std::ctype_base::alnum, c) || "+-."sv.find(c) != std::string_view::npos;
    });
    return notSchemeChar == str.end();
}

bool isNid(std::string_view str)
{
    using namespace std::string_view_literals;
    if (str.empty() || str.size() > 32 || ! classicCtype().is(std::ctype_base::alnum, str[0]))
    {
        return false;
    }
    const char* notNidChar = std::ranges::find_if_not(str, [](const char c) {
        return classicCtype().is(std::ctype_base::alnum, c) || c == '-';
    });
    return notNidChar == str.end();
}


}// anonymous namespace

bool Element::Identity::Scheme::isHttpLike() const
{
    static const std::set<std::string_view> httpLike{"http", "https"};
    return httpLike.contains(*this);
}

std::weak_ordering Element::Identity::Scheme::operator<=>(const Scheme& rhs) const
{
    auto baseCmp = static_cast<const std::string&>(*this) <=> static_cast<const std::string&>(rhs);
    if (is_eq(baseCmp))
    {
        return baseCmp;
    }
    if (isHttpLike() && rhs.isHttpLike())
    {
        return std::weak_ordering::equivalent;
    }
    return baseCmp;
}

bool fhirtools::Element::Identity::empty() const
{
    return pathOrId.empty() && ! scheme.has_value() && ! containedId.has_value();
}

std::string fhirtools::Element::Identity::url() const
{
    std::ostringstream oss;
    if (scheme)
    {
        oss << *scheme << ':';
    }
    oss << pathOrId;
    return std::move(oss).str();
}

bool Element::Identity::Scheme::operator==(const Element::Identity::Scheme& rhs) const
{
    return is_eq(*this <=> rhs);
}

bool Element::Identity::operator==(const Element::Identity& rhs) const
{
    return is_eq(*this <=> rhs);
}

Element::IdentityAndResult Element::IdentityAndResult::fromReferenceString(std::string_view referenceString,
                                                                           std::string_view elementFullPath)
{
    using namespace std::string_view_literals;
    using std::max;
    using std::min;
    auto warn = [&](std::string msg) {
        ValidationResults error;
        error.add(Severity::warning, msg.append(referenceString), std::string{elementFullPath}, nullptr);
        return IdentityAndResult{.result{error}};
    };
    IdentityAndResult result;
    const char* pathOrIdEnd = referenceString.end();
    const char* queryEnd = referenceString.end();
    const char* schemeEnd = referenceString.end();
    const char* pos = std::ranges::find_first_of(referenceString, ":?#");
    if (pos != referenceString.end() && *pos == ':')
    {
        schemeEnd = pos;
    }
    pathOrIdEnd = std::ranges::find_if(pos, referenceString.end(), [](const char c) {
        return "?#"sv.find(c) != std::string_view::npos;
    });
    queryEnd = std::ranges::find(pathOrIdEnd, referenceString.end(), '#');
    if (schemeEnd == referenceString.end())
    {
        // relative uri
        result.identity.pathOrId = std::string_view{referenceString.begin(), pathOrIdEnd};
    }
    else
    {
        std::string_view scheme{referenceString.begin(), schemeEnd};
        if (! isScheme(scheme))
        {
            return warn("invalid scheme in reference: ");
        }
        if (scheme == "urn")
        {
            // https://www.rfc-editor.org/rfc/rfc2141
            const char* nidEnd = std::ranges::find(schemeEnd + 1, pathOrIdEnd, ':');
            if (nidEnd == pathOrIdEnd || nidEnd <= schemeEnd + 1)
            {
                return warn("missing NID in URN: ");
            }
            const std::string_view nid{schemeEnd + 1, nidEnd};
            if (! isNid(nid))
            {
                return warn("invalid NID in URN: ");
            }
            scheme = std::string_view{referenceString.begin(), nidEnd};
            result.identity.pathOrId = std::string_view{nidEnd + 1, pathOrIdEnd};
        }
        else
        {
            result.identity.pathOrId = std::string_view{schemeEnd + 1, pathOrIdEnd};
        }
        result.identity.scheme.emplace(scheme);
    }
    if (queryEnd != referenceString.end())
    {
        result.identity.containedId.emplace(queryEnd + 1, referenceString.end());
    }
    return result;
}

std::ostream& fhirtools::operator<<(std::ostream& os, const Element::Identity& ri)
{
    if (ri.scheme)
    {
        os << *ri.scheme << ':';
    }
    os << ri.pathOrId;
    if (ri.containedId)
    {
        os << '#' << *ri.containedId;
    }
    return os;
}

std::string fhirtools::to_string(const Element::Identity& ri)
{
    std::ostringstream oss;
    oss << ri;
    return oss.str();
}

Element::Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
                 ProfiledElementTypeInfo definitionPointer)
    : mFhirStructureRepository(fhirStructureRepository)
    , mDefinitionPointer(std::move(definitionPointer))
    , mParent(std::move(parent))
    , mType(GetElementType(fhirStructureRepository, mDefinitionPointer))
{
    Expect3(mFhirStructureRepository != nullptr, "fhirStructureRepository must not be nullptr", std::logic_error);
}

Element::Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
                 const std::string& elementId)
    : mFhirStructureRepository(fhirStructureRepository)
    , mDefinitionPointer(fhirStructureRepository, elementId)
    , mParent(std::move(parent))
    , mType(GetElementType(fhirStructureRepository, mDefinitionPointer))
{
    Expect3(mFhirStructureRepository != nullptr, "fhirStructureRepository must not be nullptr", std::logic_error);
}


Element::Type Element::GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                      const ProfiledElementTypeInfo& definitionPointer)
{
    auto GetElementType = [](const FhirStructureDefinition* structureDefinition) {
        switch (structureDefinition->kind())
        {
            case FhirStructureDefinition::Kind::primitiveType:
            case FhirStructureDefinition::Kind::resource:
            case FhirStructureDefinition::Kind::logical:
            case FhirStructureDefinition::Kind::slice:
                return Type::Structured;
            case FhirStructureDefinition::Kind::complexType:
                if (structureDefinition->typeId() == "Quantity")
                {
                    return Type::Quantity;
                }
                else
                {
                    return Type::Structured;
                }
                break;
            case FhirStructureDefinition::Kind::systemBoolean:
                return Type::Boolean;
            case FhirStructureDefinition::Kind::systemString:
                return Type::String;
            case FhirStructureDefinition::Kind::systemDouble:
                return Type::Decimal;
            case FhirStructureDefinition::Kind::systemInteger:
                return Type::Integer;
            case FhirStructureDefinition::Kind::systemDate:
                return Type::Date;
            case FhirStructureDefinition::Kind::systemTime:
                return Type::Time;
            case FhirStructureDefinition::Kind::systemDateTime:
                return Type::DateTime;
        }
        FPFail("unhandled kind " + std::to_string(uintmax_t(structureDefinition->kind())));
    };

    const auto& element = definitionPointer.element();
    if (element && ! definitionPointer.profile()->isSystemType())
    {
        const auto* typeStructure = fhirStructureRepository->findTypeById(element->typeId());
        if (element && typeStructure->kind() == FhirStructureDefinition::Kind::primitiveType)
        {
            auto value = typeStructure->findElement(element->typeId() + ".value");
            FPExpect(value, "value element not found for primitive type " + element->typeId());
            const auto* valueTypeStructure = fhirStructureRepository->findTypeById(value->typeId());
            FPExpect(valueTypeStructure, "structure definition not found for " + value->typeId());
            return GetElementType(valueTypeStructure);
        }
        return GetElementType(typeStructure);
    }
    return GetElementType(definitionPointer.profile());
}

Element::Type Element::GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                      const FhirStructureDefinition* structureDefinition, const std::string& elementId)
{
    auto fhirElement = structureDefinition->findElement(elementId);
    FPExpect(fhirElement != nullptr,
             structureDefinition->url() + '|' + structureDefinition->version() + " no such element: " + elementId);
    return GetElementType(fhirStructureRepository, ProfiledElementTypeInfo{structureDefinition, fhirElement});
}


const fhirtools::FhirStructureRepository* Element::getFhirStructureRepository() const
{
    return mFhirStructureRepository;
}

const fhirtools::FhirStructureDefinition* Element::getStructureDefinition() const
{
    return mDefinitionPointer.profile();
}

const fhirtools::ProfiledElementTypeInfo& Element::definitionPointer() const
{
    return mDefinitionPointer;
}

Element::IdentityAndResult Element::resourceIdentity(std::string_view elementFullPath, bool allowResourceId) const
{
    ValidationResults failList;
    const auto& resourceRoot = this->resourceRoot();
    Expect3(resourceRoot != nullptr, "Cannot handle references outside resources", std::logic_error);
    auto bundledId = resourceRoot->bundledResourceIdentity(elementFullPath);
    if (! bundledId.identity.pathOrId.empty())
    {
        return bundledId;
    }
    failList.merge(std::move(bundledId.result));
    auto containedId = resourceRoot->containedIdentity(allowResourceId, elementFullPath);
    if (containedId.identity.containedId.has_value())
    {
        return containedId;
    }
    failList.merge(std::move(containedId.result));
    auto metaSourceId = resourceRoot->metaSourceIdentity(elementFullPath);
    if (! metaSourceId.identity.pathOrId.empty())
    {
        return metaSourceId;
    }
    failList.merge(std::move(metaSourceId.result));
    if (allowResourceId)
    {
        auto resourceId = resourceRoot->resourceTypeIdIdentity();
        if (! resourceId.identity.pathOrId.empty())
        {
            return resourceId;
        }
        failList.merge(resourceId.result);
    }
    return {{}, std::move(failList)};
}

Element::IdentityAndResult Element::metaSourceIdentity(std::string_view elementFullPath) const
{
    FPExpect3(isResource(), "metaSourceIdentity called on non-resource", std::logic_error);
    const auto& meta = subElements("meta");
    if (meta.empty())
    {
        return {};
    }
    const auto& source = meta[0]->subElements("source");
    if (source.empty())
    {
        return {};
    }
    return IdentityAndResult::fromReferenceString(source[0]->asString(), elementFullPath);
}


Element::IdentityAndResult Element::bundledResourceIdentity(std::string_view elementFullPath) const
{
    const auto& parent = this->parent();
    if (! parent || parent->mDefinitionPointer.element()->name() != "Bundle.entry")
    {
        ValidationResults validationResult;
        validationResult.add(Severity::debug, "Element is not in a bundle", std::string{elementFullPath}, nullptr);
        return {{}, std::move(validationResult)};
    }
    const auto& fullUrlElement = parent->subElements("fullUrl");
    if (fullUrlElement.empty())
    {
        ValidationResults validationResult;
        validationResult.add(Severity::debug, "fullUrl not set.", std::string{elementFullPath}, nullptr);
        return {.result{std::move(validationResult)}};
    }
    return IdentityAndResult::fromReferenceString(fullUrlElement[0]->asString(), elementFullPath);
}

Element::IdentityAndResult Element::containedIdentity(bool allowResourceId, std::string_view elementFullPath) const
{
    const auto& parent = this->parent();
    const auto& id = subElements("id");
    if (! parent || id.empty() || ! parent->isContainerResource())
    {
        return {};
    }
    auto idRes = parent->bundledResourceIdentity(elementFullPath);
    if (idRes.identity.pathOrId.empty())
    {
        idRes = parent->metaSourceIdentity(elementFullPath);
        if (idRes.identity.pathOrId.empty() && allowResourceId)
        {
            idRes = parent->resourceTypeIdIdentity();
        }
    }
    if (! idRes.identity.pathOrId.empty())
    {
        idRes.identity.containedId = id[0]->asString();
    }
    return idRes;
}

Element::IdentityAndResult Element::resourceTypeIdIdentity() const
{
    const auto& resourceType = this->resourceType();
    const auto& idElement = subElements("id");
    if (idElement.empty())
    {
        return {.identity{.pathOrId = resourceType}};
    }
    const auto& id = idElement[0]->asString();
    Identity result;
    result.pathOrId.reserve(id.size() + resourceType.size() + 1);
    result.pathOrId.append(resourceType).append(1, '/').append(id);
    return {result, {}};
}


Element::IdentityAndResult Element::referenceTargetIdentity(std::string_view elementFullPath) const
{
    if (mType == Type::String)
    {
        return referenceTargetIdentity(IdentityAndResult::fromReferenceString(asString(), elementFullPath),
                                       elementFullPath);
    }
    if (mType == Type::Structured && definitionPointer().element()->typeId() == "Reference")
    {
        const auto& referenceField = subElements("reference");
        if (referenceField.empty())
        {
            IdentityAndResult result;
            result.result.add(Severity::debug, "Reference is not a url reference", std::string{elementFullPath},
                              nullptr);
            return result;
        }
        return referenceTargetIdentity(
            IdentityAndResult::fromReferenceString(referenceField[0]->asString(), elementFullPath), elementFullPath);
    }
    IdentityAndResult result;
    result.result.add(Severity::error, "Not a reference element type: " + definitionPointer().element()->typeId(),
                      std::string{elementFullPath}, nullptr);
    return result;
}

Element::IdentityAndResult Element::referenceTargetIdentity(IdentityAndResult reference,
                                                            std::string_view elementFullPath) const
{

    if (reference.identity.scheme)
    {
        return reference;
    }
    if (reference.identity.empty())
    {
        IdentityAndResult result;
        result.result.add(Severity::debug, "empty reference", std::string{elementFullPath}, nullptr);
        return result;
    }
    const auto& resourceRoot = this->resourceRoot();
    if (! resourceRoot)
    {
        reference.result.add(Severity::debug, "Reference outside resource", std::string{elementFullPath}, nullptr);
        return reference;
    }
    if (reference.identity.pathOrId.empty())
    {
        auto ownIdentity = resourceRoot->resourceIdentity(elementFullPath);
        reference.identity.scheme = ownIdentity.identity.scheme;
        reference.identity.pathOrId = ownIdentity.identity.pathOrId;
        reference.result.merge(std::move(ownIdentity.result));
        if (reference.identity.containedId && reference.identity.containedId->empty())
        {
            reference.identity.containedId.reset();
        }
        return reference;
    }
    const size_t slashPos = reference.identity.pathOrId.find('/');
    if (slashPos != std::string::npos)
    {
        const auto& resourceTypes = mFhirStructureRepository->findCodeSystem(constants::resourceTypesUrl, std::nullopt);
        const std::string_view resourceType{reference.identity.pathOrId.begin(),
                                      reference.identity.pathOrId.begin() + gsl::narrow<std::string_view::difference_type>(slashPos)};
        if (resourceTypes->containsCode(resourceType))
        {
            return relativeReferenceTargetIdentity(std::move(reference), resourceRoot, elementFullPath);
        }
    }
    reference.result.add(Severity::debug, "invalid reference: " + to_string(reference.identity),
                         std::string{elementFullPath}, nullptr);
    reference.identity = {};
    return reference;
}
Element::IdentityAndResult
Element::relativeReferenceTargetIdentity(IdentityAndResult&& reference,
                                         const std::shared_ptr<const Element>& currentResoureRoot,
                                         std::string_view elementFullPath) const
{
    static const std::set<std::string_view> httpLike{"http", "https"};
    using namespace std::string_literals;
    using std::max;
    auto ownIdentity = currentResoureRoot->resourceIdentity(elementFullPath, false);
    if (ownIdentity.identity.pathOrId.empty() || ! ownIdentity.identity.scheme ||
        ! httpLike.contains(*ownIdentity.identity.scheme))
    {
        reference.result.add(Severity::debug, "cannot derive full url", std::string{elementFullPath}, nullptr);
        return std::move(reference);
    }
    std::string_view ownFullUrlPart{ownIdentity.identity.pathOrId};
    const size_t lastSlash = ownFullUrlPart.rfind('/');
    const size_t prevSlash = lastSlash > 0 ? ownFullUrlPart.rfind('/', lastSlash - 1) : std::string_view::npos;
    if (lastSlash == std::string_view::npos || prevSlash == std::string_view::npos)
    {
        reference.result.add(Severity::debug, "Cannot derive prefix from url: " + ownIdentity.identity.pathOrId,
                             std::string{elementFullPath}, nullptr);
        return std::move(reference);
    }
    const auto& prefix = ownFullUrlPart.substr(0, prevSlash);
    const auto& resourceInOwnUrl = ownFullUrlPart.substr(prevSlash + 1, lastSlash - prevSlash - 1);
    const auto* resourceProfile = currentResoureRoot->definitionPointer().profile();
    const auto& ownTypeId = resourceProfile->typeId();
    // neither equals to ownTypeId nor ends_with '/' + ownTypeId
    if (resourceInOwnUrl != ownTypeId)
    {
        IdentityAndResult result;
        result.result.add(
            Severity::debug,
            "Resources fullUrl is not RESTful as it doesn't match resource type '"s.append(resourceInOwnUrl)
                .append("' != '")
                .append(ownTypeId)
                .append("': ")
                .append(ownIdentity.identity.pathOrId),
            std::string{elementFullPath}, nullptr);
        return result;
    }
    IdentityAndResult result;
    result.identity.scheme = ownIdentity.identity.scheme;
    result.identity.pathOrId.reserve(prefix.size() + reference.identity.pathOrId.size() + 1);
    result.identity.pathOrId.append(prefix).append(1, '/').append(reference.identity.pathOrId);
    result.identity.containedId = reference.identity.containedId;
    result.result.add(Severity::debug, to_string(reference.identity) + " completed to " + to_string(result.identity),
                      std::string{elementFullPath}, nullptr);
    return result;
}


std::tuple<std::shared_ptr<const Element>, ValidationResults>
Element::resolveReference(std::string_view elementFullPath) const
{
    std::optional<IdentityAndResult> identity;
    if (mType == Type::String)
    {
        identity = IdentityAndResult::fromReferenceString(asString(), elementFullPath);
    }
    if (mType == Type::Structured && definitionPointer().element()->typeId() == "Reference")
    {
        const auto& referenceField = subElements("reference");
        if (referenceField.empty())
        {
            ValidationResults result;
            result.add(Severity::debug, "Reference is not a url reference", std::string{elementFullPath}, nullptr);
            return {nullptr, std::move(result)};
        }
        identity = IdentityAndResult::fromReferenceString(referenceField[0]->asString(), elementFullPath);
    }
    if (identity)
    {
        auto resolved = resolveReference(identity->identity, elementFullPath);
        get<ValidationResults>(resolved).merge(std::move(identity->result));
        return resolved;
    }
    return {};
}

std::tuple<std::shared_ptr<const Element>, ValidationResults>
Element::resolveReference(const Identity& reference, std::string_view elementFullPath) const
{
    ValidationResults resultList;
    if (reference.containedId.has_value())
    {
        std::shared_ptr<const Element> referencedContainer;
        if (reference.pathOrId.empty())
        {
            referencedContainer = containerResource();
        }
        else
        {
            std::tie(referencedContainer, resultList) = resolveUrlReference(reference, elementFullPath);
        }
        if (referencedContainer)
        {
            auto result = referencedContainer->resolveContainedReference(*reference.containedId);
            get<ValidationResults>(result).merge(std::move(resultList));
            return result;
        }
        resultList.add(Severity::debug, "reference target not found: " + to_string(reference),
                       std::string{elementFullPath}, nullptr);
        return std::make_tuple(nullptr, std::move(resultList));
    }
    if (reference.pathOrId.empty())
    {
        resultList.add(Severity::debug, "reference target not found: " + to_string(reference),
                       std::string{elementFullPath}, nullptr);
        return std::make_tuple(nullptr, std::move(resultList));
    }
    return resolveUrlReference(reference, elementFullPath);
}

std::tuple<std::shared_ptr<const Element>, ValidationResults>
Element::resolveUrlReference(const Identity& urlIdentity, std::string_view elementFullPath) const
{
    using namespace std::string_literals;
    auto fullTargetRef = referenceTargetIdentity({urlIdentity}, elementFullPath);
    const auto containingBundle = this->containingBundle();
    if (containingBundle)
    {
        auto resolution = containingBundle->resolveBundleReference(fullTargetRef.identity.url(), elementFullPath);
        fullTargetRef.result.merge(std::move(get<ValidationResults>(resolution)));
        return {std::move(get<std::shared_ptr<const Element>>(resolution)), std::move(fullTargetRef.result)};
    }
    auto ownIdentity = resourceIdentity(elementFullPath);
    if (ownIdentity.identity.pathOrId == fullTargetRef.identity.pathOrId &&
        ownIdentity.identity.scheme == fullTargetRef.identity.scheme)
    {
        if (ownIdentity.identity.containedId.has_value())
        {
            auto containerResource = this->containerResource();
            if (containerResource == nullptr)
            {
                ValidationResults resultList;
                resultList.add(Severity::error,
                               "Reference to non-existent container resource: "s.append(to_string(urlIdentity)),
                               std::string{elementFullPath}, nullptr);
                return {nullptr, std::move(resultList)};
            }
            return std::make_tuple(std::move(containerResource), ValidationResults{});
        }
        return {resourceRoot(), {}};
    }
    ValidationResults resultList;
    resultList.add(Severity::debug, "reference target not found: "s.append(to_string(urlIdentity)),
                   std::string{elementFullPath}, nullptr);
    return {nullptr, std::move(resultList)};
}

std::tuple<std::shared_ptr<const Element>, ValidationResults>
fhirtools::Element::resolveBundleReference(std::string_view fullUrl, std::string_view elementFullPath) const
{
    using namespace std::string_literals;
    for (const auto& entry : subElements("entry"))
    {
        const auto& fullUrlElement = entry->subElements("fullUrl");
        if (! fullUrlElement.empty() && fullUrlElement[0]->asString() == fullUrl)
        {
            const auto& resource = entry->subElements("resource");
            if (resource.empty())
            {
                ValidationResults resultList;
                resultList.add(Severity::error, "missing resource in referenced resource entry",
                               std::string{elementFullPath}, nullptr);
                return {nullptr, std::move(resultList)};
            }
            return {resource[0], {}};
        }
    }
    ValidationResults resultList;
    resultList.add(Severity::debug, "reference target not found: "s.append(fullUrl), std::string{elementFullPath},
                   nullptr);
    return {nullptr, std::move(resultList)};
}

std::tuple<std::shared_ptr<const Element>, ValidationResults>
fhirtools::Element::resolveContainedReference(std::string_view containedId) const
{
    ValidationResults resultList;
    for (const auto& contained : subElements("contained"))
    {
        const auto& id = contained->subElements("id");
        if (id.empty())
        {
            resultList.add(Severity::error, "No id in contained resource", "<unknown>", nullptr);
            continue;
        }
        if (id[0]->asString() == containedId)
        {
            return {contained, std::move(resultList)};
        }
    }
    return {nullptr, std::move(resultList)};
}

// NOLINTNEXTLINE(misc-no-recursion)
std::shared_ptr<const Element> fhirtools::Element::documentRoot() const
{
    const auto& parent = this->parent();
    if (! parent)
    {
        return shared_from_this();
    }
    return parent->documentRoot();
}

std::shared_ptr<const Element> fhirtools::Element::resourceRootParent() const
{
    const auto& resourceRoot = this->resourceRoot();
    return resourceRoot ? resourceRoot->parent() : nullptr;
}

std::shared_ptr<const Element> Element::parentResource() const
{
    const auto& resourceParent = this->resourceRootParent();
    return resourceParent ? resourceParent->resourceRoot() : nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::shared_ptr<const Element> fhirtools::Element::resourceRoot() const
{
    if (isResource())
    {
        return shared_from_this();
    }
    const auto& p = parent();
    return p ? p->resourceRoot() : nullptr;
}

std::shared_ptr<const Element> fhirtools::Element::containerResource() const
{
    const auto& resourceRoot = this->resourceRoot();
    const auto& resourceParent = resourceRoot ? resourceRoot->parent() : nullptr;
    if (resourceParent && resourceParent->hasSubElement("contained") &&
        resourceParent->definitionPointer().profile()->isDerivedFrom(*mFhirStructureRepository,
                                                                     constants::domainResourceUrl))
    {
        return resourceParent->resourceRoot();
    }
    if (resourceRoot->definitionPointer().profile()->isDerivedFrom(*mFhirStructureRepository,
                                                                   constants::domainResourceUrl))
    {
        return resourceRoot;
    }
    return nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::shared_ptr<const Element> fhirtools::Element::containingBundle() const
{
    const auto& resourceParent = this->resourceRootParent();
    if (! resourceParent)
    {
        return nullptr;
    }
    if (resourceParent->definitionPointer().profile()->isDerivedFrom(*mFhirStructureRepository, constants::bundleUrl))
    {
        return resourceParent->resourceRoot();
    }
    return resourceParent->containingBundle();
}

void Element::setIsContextElement(const std::shared_ptr<IsContextElementTag>& tag) const
{
    mIsContextElementTag = tag;
}

bool Element::isContextElement() const
{
    return mIsContextElementTag.lock() != nullptr;
}

Element::Type Element::type() const
{
    return mType;
}

std::shared_ptr<const Element> Element::parent() const
{
    return mParent.lock();
}

bool Element::isResource() const
{
    return mDefinitionPointer.isResource();
}

bool Element::isContainerResource() const
{
    return mDefinitionPointer.isResource() && hasSubElement("contained");
}

// NOLINTNEXTLINE(misc-no-recursion)
std::optional<std::strong_ordering> fhirtools::Element::compareTo(const Element& rhs) const
{
    if (! isImplicitConvertible(type(), rhs.type()) && isImplicitConvertible(rhs.type(), type()))
    {
        auto reverseResult = rhs.compareTo(*this);
        if (! reverseResult.has_value() || std::is_eq(*reverseResult))
        {
            return reverseResult;
        }
        else if (reverseResult == std::strong_ordering::greater)
        {
            return std::make_optional(std::strong_ordering::less);
        }
        else if (reverseResult == std::strong_ordering::less)
        {
            return std::make_optional(std::strong_ordering::greater);
        }
        else
        {
            FPFail2("unexpected std::strong_ordering value", std::logic_error);
        }
    }
    FPExpect(isImplicitConvertible(type(), rhs.type()), "incompatible operands for comparison");
    switch (rhs.type())
    {
        case Type::Integer:
            return asInt() <=> rhs.asInt();
        case Type::Decimal:
            if (asDecimal() < rhs.asDecimal())
            {
                return std::strong_ordering::less;
            }
            else if (asDecimal() > rhs.asDecimal())
            {
                return std::strong_ordering::greater;
            }
            else
            {
                return std::strong_ordering::equal;
            }
        case Type::String:
            return asString() <=> rhs.asString();
        case Type::Date:
            return asDate().compareTo(rhs.asDate());
        case Type::DateTime:
            return asDateTime().compareTo(rhs.asDateTime());
        case Type::Time:
            return asTime().compareTo(rhs.asTime());
        case Type::Quantity:
            return asQuantity().compareTo(rhs.asQuantity());
        case Type::Boolean:
            return asBool() <=> rhs.asBool();
        case Type::Structured:
            break;
    }
    FPFail("invalid type for comparison");
}

std::optional<bool> fhirtools::Element::equals(const Element& rhs) const
{
    try
    {
        switch (rhs.type())
        {
            case Element::Type::Integer:
            case Element::Type::Decimal:
            case Element::Type::String:
            case Element::Type::Boolean:
            case Element::Type::Date:
            case Element::Type::DateTime:
            case Element::Type::Time:
            case Element::Type::Quantity: {
                const auto compareToResult = compareTo(rhs);
                return compareToResult.has_value() ? std::make_optional(compareToResult == std::strong_ordering::equal)
                                                   : std::nullopt;
            }
            case Element::Type::Structured: {
                const auto subElementsLhs = subElementNames();
                const auto subElementsRhs = rhs.subElementNames();
                if (subElementsLhs != subElementsRhs)
                {
                    return false;
                }
                return std::ranges::all_of(subElementsRhs, [&rhs, this](const auto& item) {
                    const Collection subElementLhs{subElements(item)};
                    const Collection subElementRhs{rhs.subElements(item)};
                    return subElementLhs.equals(subElementRhs) == true;
                });
            }
        }
    }
    catch (const std::exception&)
    {
    }
    return false;
}

//NOLINTNEXTLINE(misc-no-recursion)
bool Element::matches(const Element& pattern) const
{
    Expect3(pattern.type() == type(), "type mismatch", std::logic_error);
    if (type() == Type::Structured)
    {
        const auto& subNames = subElementNames();
        for (const auto& patternSubName : pattern.subElementNames())
        {
            if (std::ranges::find(subNames, patternSubName) == subNames.end())
            {
                return false;
            }
            for (const auto& patternSub : pattern.subElements(patternSubName))
            {
                //NOLINTNEXTLINE(misc-no-recursion)
                bool found = std::ranges::any_of(subElements(patternSubName), [&patternSub](const auto& sub) {
                    return sub->matches(*patternSub);
                });
                if (! found)
                {
                    return false;
                }
            }
        }
        return true;
    }
    return equals(pattern) == true;
}

Element::QuantityType::QuantityType(DecimalType value, const std::string_view& unit)
    : mValue(std::move(value))
    , mUnit(unit)
{
}

Element::QuantityType::QuantityType(const std::string_view& valueAndUnit)
{
    size_t idx = 0;
    (void) std::stod(std::string(valueAndUnit), &idx);
    mValue = DecimalType(std::string_view{valueAndUnit.data(), idx});
    mUnit = boost::trim_copy(std::string(valueAndUnit.substr(idx)));
}

std::optional<std::strong_ordering> fhirtools::Element::QuantityType::compareTo(const Element::QuantityType& rhs) const
{
    if (! convertibleToUnit(rhs.mUnit))
    {
        return std::nullopt;
    }
    const auto convertedUnit = convertToUnit(rhs.mUnit);
    if (convertedUnit.mValue < rhs.mValue)
    {
        return std::strong_ordering::less;
    }
    else if (convertedUnit.mValue > rhs.mValue)
    {
        return std::strong_ordering::greater;
    }
    else
    {
        return std::strong_ordering::equal;
    }
}
std::optional<bool> fhirtools::Element::QuantityType::equals(const Element::QuantityType& rhs) const
{
    const auto result = compareTo(rhs);
    if (! result.has_value())
    {
        return std::nullopt;
    }
    return result == std::strong_ordering::equal;
}

Element::QuantityType Element::QuantityType::convertToUnit(const std::string& unit) const
{
    FPExpect(mUnit == unit, "unit " + mUnit + " not convertible to " + unit);
    return *this;
}

bool Element::QuantityType::convertibleToUnit(const std::string& unit) const
{
    return mUnit == unit;
}

PrimitiveElement::PrimitiveElement(const FhirStructureRepository* fhirStructureRepository, Type type, ValueType&& value)
    : Element(fhirStructureRepository, {},
              ProfiledElementTypeInfo{GetStructureDefinition(fhirStructureRepository, type)})
    , mValue(value)
{
    using namespace std::string_literals;
    struct TypeChecker {
        TypeChecker(Type type)
            : mType(type)
        {
        }
        void operator()(int32_t)
        {
            FPExpect(mType == Element::Type::Integer,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Integer");
        }
        void operator()(const DecimalType&)
        {
            FPExpect(mType == Element::Type::Decimal,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Decimal");
        }
        void operator()(bool)
        {
            FPExpect(mType == Element::Type::Boolean,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Boolean");
        }
        void operator()(const std::string&)
        {
            FPExpect(mType == Element::Type::String,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/String");
        }
        void operator()(const QuantityType&)
        {
            FPExpect(mType == Element::Type::Quantity,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Quantity");
        }
        void operator()(const Date&)
        {
            FPExpect(mType == Element::Type::Date,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Date");
        }
        void operator()(const DateTime&)
        {
            FPExpect(mType == Element::Type::DateTime,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/DateTime");
        }
        void operator()(const Time&)
        {
            FPExpect(mType == Element::Type::Time,
                     "type/value mismatch "s.append(magic_enum::enum_name(mType)) + "/Time");
        }
        Type mType;
    };
    std::visit(TypeChecker(type), mValue);
}

const fhirtools::FhirStructureDefinition*
PrimitiveElement::GetStructureDefinition(const FhirStructureRepository* fhirStructureRepository, Element::Type type)
{
    switch (type)
    {
        case Type::Integer:
            return fhirStructureRepository->systemTypeInteger();
        case Type::Decimal:
            return fhirStructureRepository->systemTypeDecimal();
        case Type::String:
            return fhirStructureRepository->systemTypeString();
        case Type::Boolean:
            return fhirStructureRepository->systemTypeBoolean();
        case Type::Date:
            return fhirStructureRepository->systemTypeDate();
        case Type::DateTime:
            return fhirStructureRepository->systemTypeDateTime();
        case Type::Time:
            return fhirStructureRepository->systemTypeTime();
        case Type::Quantity:
            return fhirStructureRepository->findTypeById("Quantity");
        case Type::Structured:
            break;
    }
    FPFail("unsupported type for PrimitiveElement");
}

int32_t PrimitiveElement::asInt() const
{
    switch (type())
    {
        case Type::Integer:
            return std::get<int32_t>(mValue);
        case Type::String: {
            const auto& asString = std::get<std::string>(mValue);
            size_t idx = 0;
            auto asInt = std::stoi(asString, &idx);
            if (idx == asString.size())
                return asInt;
            break;
        }
        case Type::Boolean:
            return std::get<bool>(mValue) ? 1 : 0;
        case Type::Decimal:
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to int");
}

bool fhirtools::PrimitiveElement::hasSubElement(const std::string&) const
{
    return false;
}

std::vector<std::string> PrimitiveElement::subElementNames() const
{
    return {};
}

bool fhirtools::PrimitiveElement::hasValue() const
{
    return true;
}

std::vector<fhirtools::Collection::value_type> PrimitiveElement::subElements(const std::string&) const
{
    return {};
}

std::string PrimitiveElement::resourceType() const
{
    return {};
}

std::vector<std::string_view> PrimitiveElement::profiles() const
{
    return {};
}

std::string PrimitiveElement::asString() const
{
    switch (type())
    {

        case Type::Integer:
            return std::to_string(std::get<int32_t>(mValue));
        case Type::Decimal:
            return decimalAsString(std::get<DecimalType>(mValue));
        case Type::String:
            return std::get<std::string>(mValue);
        case Type::Boolean:
            return std::get<bool>(mValue) ? "true" : "false";
        case Type::Structured:
            break;
        case Type::Date:
            return std::get<Date>(mValue).toString();
        case Type::DateTime:
            return std::get<DateTime>(mValue).toString();
        case Type::Time:
            return std::get<Time>(mValue).toString();
        case Type::Quantity: {
            const auto& q = std::get<QuantityType>(mValue);
            return decimalAsString(q.value()) + " " + q.unit();
        }
    }
    FPFail("not convertible to string");
}

std::string PrimitiveElement::decimalAsString(const DecimalType& dec) const
{
    auto trimmed =
        boost::trim_right_copy_if(dec.str(8, std::ios_base::fixed | std::ios_base::showpoint), [](const auto& c) {
            return c == '0';
        });
    if (trimmed.back() == '.')
        trimmed += '0';
    return trimmed;
}

fhirtools::DecimalType PrimitiveElement::asDecimal() const
{
    switch (type())
    {
        case Type::Decimal:
            return std::get<DecimalType>(mValue);
        case Type::Integer:
            return std::get<int32_t>(mValue);
        case Type::Boolean:
            return std::get<bool>(mValue) ? DecimalType("1.0") : DecimalType("0.0");
        case Type::String:
            return DecimalType(std::get<std::string>(mValue));
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to decimal");
}

bool PrimitiveElement::asBool() const
{
    switch (type())
    {
        case Type::Boolean:
            return std::get<bool>(mValue);
        case Type::Integer: {
            const auto& asInt = std::get<int32_t>(mValue);
            if (asInt == 0)
                return false;
            if (asInt == 1)
                return true;
            break;
        }
        case Type::Decimal: {
            const auto& asDecimal = std::get<DecimalType>(mValue);
            if (asDecimal == 0)
                return false;
            if (asDecimal == 1)
                return true;
            break;
        }
        case Type::String: {
            auto asString = boost::to_lower_copy(std::get<std::string>(mValue));
            if (asString == "true" || asString == "t" || asString == "yes" || asString == "y" || asString == "1" ||
                asString == "1.0")
                return true;
            if (asString == "false" || asString == "f" || asString == "no" || asString == "n" || asString == "0" ||
                asString == "0.0")
                return false;
            break;
        }
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to bool");
}

fhirtools::Date PrimitiveElement::asDate() const
{
    switch (type())
    {
        case Type::Date:
            return std::get<Date>(mValue);
        case Type::DateTime:
            return std::get<DateTime>(mValue).date();
        case Type::String:
            return Date(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Time:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Date");
}

fhirtools::Time PrimitiveElement::asTime() const
{
    switch (type())
    {
        case Type::Time:
            return std::get<Time>(mValue);
        case Type::String:
            return Time(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Date:
        case Type::DateTime:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Time");
}

fhirtools::DateTime PrimitiveElement::asDateTime() const
{
    switch (type())
    {
        case Type::Date:
            return DateTime(std::get<Date>(mValue));
        case Type::DateTime:
            return std::get<DateTime>(mValue);
        case Type::String:
            return DateTime(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Time:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to DateTime");
}

Element::QuantityType PrimitiveElement::asQuantity() const
{
    switch (type())
    {
        case Type::Integer:
            return QuantityType(std::get<int32_t>(mValue), "");
        case Type::Decimal:
            return QuantityType(std::get<DecimalType>(mValue), "");
        case Type::String:
            return QuantityType(std::get<std::string>(mValue));
        case Type::Boolean:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
            break;
        case Type::Quantity:
            return std::get<QuantityType>(mValue);
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Quantity");
}


bool fhirtools::isImplicitConvertible(Element::Type from, Element::Type to)
{
    if (from == to)
    {
        return true;
    }
    if (to == Element::Type::String)
    {
        // Although not allowed in the spec, always allow implicit conversion to string, because the HAPI also behaves
        // that way.
        return true;
    }
    switch (from)
    {
        case Element::Type::Integer:
            return to == Element::Type::Decimal || to == Element::Type::Quantity;
        case Element::Type::Decimal:
            return to == Element::Type::Quantity;
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Structured:
            break;
        case Element::Type::Date:
            return to == Element::Type::DateTime;
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Quantity:
            break;
    }
    return false;
}

std::ostream& fhirtools::operator<<(std::ostream& os, Element::Type type)
{
    os << magic_enum::enum_name(type);
    return os;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::ostream& fhirtools::operator<<(std::ostream& os, const Element& element)
{
    os << "{";
    if (auto el = element.definitionPointer().element())
    {
        os << R"("name": ")" << el->name() << R"(", )";
    }
    os << R"("type":")" << element.type() << '"';
    if (element.definitionPointer().profile()->kind() == FhirStructureDefinition::Kind::resource &&
        element.definitionPointer().element()->isRoot())
    {
        os << R"(, "resourceType":")" << element.resourceType() << '"';
    }

    os << R"(, "value":)";
    switch (element.type())
    {
        case Element::Type::Integer:
        case Element::Type::Decimal:
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Date:
        case Element::Type::DateTime:
        case Element::Type::Time:
            os << '"' << element.asString() << '"';
            break;
        case Element::Type::Quantity:
        case Element::Type::Structured: {
            os << "{";
            std::string_view sep;
            for (const auto& subElementName : element.subElementNames())
            {
                os << sep;
                sep = ", ";
                Collection c{element.subElements(subElementName)};
                os << '"' << subElementName << R"(":)";
                if (c.size() > 1)
                {
                    os << c;
                }
                else if (! c.empty())
                {
                    os << *c[0];
                }
                else
                {
                    os << "{}";
                }
            }
            os << "}";
        }
    }
    os << "}";
    return os;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::ostream& Element::json(std::ostream& out) const
{
    switch (mType)
    {
        using enum Type;
        case Integer:
        case Decimal:
        case Boolean:
            return out << asString();
        case Date:
        case DateTime:
        case Time:
        case Quantity:
        case String:
            return out << '"' << asString() << '"';
        case Structured: {
            out << '{';
            std::string_view fieldSep;
            if (definitionPointer().isResource())
            {
                out << R"("resourceType": ")" << definitionPointer().profile()->typeId();
                fieldSep = ", ";
            }
            for (const auto& name : subElementNames())
            {
                auto defPtrs = definitionPointer().subDefinitions(*getFhirStructureRepository(), name);
                out << fieldSep;
                fieldSep = ", ";
                if (defPtrs.empty())
                {
                    out << "<no-def:" << definitionPointer() << '.' << name << ">";
                    break;
                }
                auto subList = subElements(name);
                if (defPtrs.front().element()->isArray() || subList.size() != 1)
                {
                    out << '"' << name << R"(": [)";
                    std::string_view arrSep;
                    for (auto& sub : subList)
                    {
                        out << arrSep;
                        arrSep = ", ";
                        sub->json(out);
                    }
                    out << ']';
                }
                else
                {
                    out << '"' << name << R"(": )";
                    subList.back()->json(out);
                }
            }
            out << '}';
            return out;
        }
    }
    return out << "<error>";
}
