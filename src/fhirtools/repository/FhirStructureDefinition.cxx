/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirStructureDefinition.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirSlicing.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"

#include <boost/algorithm/string.hpp>
#include <ranges>
#include <unordered_set>

using namespace std::string_literals;
using fhirtools::FhirStructureDefinition;

namespace fhirtools::kind_string
{
// clang-format off
    static const std::string primitiveType = "primitive-type";
    static const std::string complexType   = "complex-type";
    static const std::string resource      = "resource";
    static const std::string logical       = "logical";
    static const std::string slice         = "slice";
    static const std::string systemBoolean = "systemBoolean";
    static const std::string systemString  = "systemString";
    static const std::string systemDouble  = "systemDouble";
    static const std::string systemInteger = "systemInteger";
    static const std::string systemDate    = "systemDate";
    static const std::string systemTime    = "systemTime";
    static const std::string systemDateTime= "systemDateTime";
// clang-format on
}

namespace fhirtools::derivation_string
{
// clang-format off
    static const std::string basetype       = "basetype";
    static const std::string specialization = "specialization";
    static const std::string constraint     = "constraint";
// clang-format on
}

FhirStructureDefinition::FhirStructureDefinition() = default;
FhirStructureDefinition::~FhirStructureDefinition() = default;

std::string fhirtools::FhirStructureDefinition::urlAndVersion() const
{
    return (std::ostringstream() << mUrl << '|' << mVersion).str();
}

fhirtools::DefinitionKey fhirtools::FhirStructureDefinition::key() const
{
    return {mUrl, mVersion};
}

const std::filesystem::path& fhirtools::FhirStructureDefinition::sourceFile() const
{
    return mSourceFile;
}

void FhirStructureDefinition::validate()
{
    Expect3(not mUrl.empty(), "Missing url in FhirStructureDefinition.", std::logic_error);
    Expect3(mResourceGroup, "Missing group in FhirStructureDefinition:" + urlAndVersion(), std::logic_error);
    Expect3(not mTypeId.empty(), "Missing type in FhirStructureDefinition: " + urlAndVersion(), std::logic_error);
}

const std::string& fhirtools::to_string(FhirStructureDefinition::Derivation derivation)
{
    using std::to_string;
    using namespace derivation_string;
    using Derivation = FhirStructureDefinition::Derivation;
    switch (derivation)
    {
        // clang-format off
        case Derivation::basetype:       return basetype;
        case Derivation::constraint:     return constraint;
        case Derivation::specialization: return specialization;
            // clang-format on
    }
    FPFail2("Invalid value for Derivation :" + to_string(static_cast<intmax_t>(derivation)), std::logic_error);
}

FhirStructureDefinition::Derivation fhirtools::stringToDerivation(const std::string_view& str)
{
    using namespace derivation_string;
    using Derivation = FhirStructureDefinition::Derivation;
    if (str == basetype)
        return Derivation::basetype;
    else if (str == constraint)
        return Derivation::constraint;
    else if (str == specialization)
        return Derivation::specialization;
    else
    {
        FPFail2("Cannot convert string to FhirStructureDefinition::Derivation: "s.append(str), std::logic_error);
    }
}

std::ostream& fhirtools::operator<<(std::ostream& out, FhirStructureDefinition::Derivation derivation)
{
    out << to_string(derivation);
    return out;
}

std::ostream& fhirtools::operator<<(std::ostream& out, const FhirStructureDefinition& structure)
{
    std::ostream xout(out.rdbuf());

    xout << R"({ "groupId": ")" << structure.resourceGroup()->id();
    xout << R"(", "typeId": ")" << structure.typeId();
    xout << R"(", "url": ")" << structure.url();
    xout << R"(", "version": ")" << structure.version();
    xout << R"(", "baseDefinition": ")" << structure.baseDefinition();
    xout << R"(", "derivation": ")" << structure.derivation();
    xout << R"(", "kind": ")" << structure.kind();
    xout << R"(", "elements": [)";
    std::string_view seperator;
    for (const auto& element : structure.elements())
    {
        xout << seperator;
        if (element)
        {
            xout << *element;
        }
        else
        {
            xout << "null";
        }
        seperator = ", ";
    }
    xout << "] }";
    return out;
}


const std::string& fhirtools::to_string(FhirStructureDefinition::Kind kind)
{
    // clang-format off
    using namespace kind_string;
    using Kind = FhirStructureDefinition::Kind;
    switch (kind)
    {
        case Kind::primitiveType: return primitiveType;
        case Kind::complexType:   return complexType;
        case Kind::resource:      return resource;
        case Kind::logical:       return logical;
        case Kind::systemBoolean: return systemBoolean;
        case Kind::systemString:  return systemString;
        case Kind::systemDouble:  return systemDouble;
        case Kind::systemInteger: return systemInteger;
        case Kind::systemDate:    return systemDate;
        case Kind::systemTime:    return systemTime;
        case Kind::systemDateTime:return systemDateTime;
        case Kind::slice:         return slice;
    }
    using std::to_string;
    Fail2("Invalid value for FhirStructureDefinition::Kind: " + to_string(static_cast<uintmax_t>(kind)),
          std::logic_error);
    // clang-format on
}

FhirStructureDefinition::Kind fhirtools::stringToKind(const std::string_view& str)
{
    using namespace kind_string;
    using Kind = FhirStructureDefinition::Kind;
    static const std::map<std::string_view, Kind> stringToKindMap{
        {primitiveType, Kind::primitiveType},
        {complexType, Kind::complexType},
        {resource, Kind::resource},
        {logical, Kind::logical},
        {systemBoolean, Kind::systemBoolean},
        {systemString, Kind::systemString},
        {systemDouble, Kind::systemDouble},
        {systemInteger, Kind::systemInteger},
        {systemDate, Kind::systemDate},
        {systemTime, Kind::systemTime},
        {systemDateTime, Kind::systemDateTime},
        {slice, Kind::slice},
    };
    try
    {
        return stringToKindMap.at(str);
    }
    catch (const std::out_of_range&)
    {
        Fail2("Cannot convert string to FhirStructureDefinition::Kind: "s.append(str), std::logic_error);
    }
}

bool FhirStructureDefinition::isSystemType() const
{
    switch (mKind)
    {
        case Kind::primitiveType:
        case Kind::complexType:
        case Kind::resource:
        case Kind::logical:
            return false;
        case Kind::systemBoolean:
        case Kind::systemString:
        case Kind::systemDouble:
        case Kind::systemInteger:
        case Kind::systemDate:
        case Kind::systemTime:
        case Kind::systemDateTime:
            return true;
        case Kind::slice:
            Fail("isSystemType is not defined for " + to_string(mKind));
    }
    using std::to_string;
    FPFail2("Ivalid value for FhirStructureDefinition::Kind: " + to_string(static_cast<uintmax_t>(mKind)),
            std::logic_error);
}

std::ostream& fhirtools::operator<<(std::ostream& out, FhirStructureDefinition::Kind kind)
{
    return (out << to_string(kind));
}

std::shared_ptr<const fhirtools::FhirElement> FhirStructureDefinition::findElement(std::string_view elementId) const
{
    return std::get<std::shared_ptr<const FhirElement>>(findElementAndIndex(elementId));
}

std::tuple<std::shared_ptr<const fhirtools::FhirElement>, size_t>
fhirtools::FhirStructureDefinition::findElementAndIndex(std::string_view elementId) const
{
    Expect3(! elementId.empty(), "elementId must not be empty.", std::logic_error);
    size_t typeSize = mTypeId.size();
    size_t cmpStart = 0;
    if (elementId[0] != '.')
    {
        if (elementId.compare(0, typeSize, mTypeId) != 0)
        {
            return {nullptr, 0};
        }
        if (typeSize == elementId.size())
        {
            return {mElements.empty() ? nullptr : rootElement(), 0};
        }
        cmpStart = typeSize;
    }
    auto element = std::ranges::find_if(mElements, [&](const std::shared_ptr<const FhirElement>& e) {
        return e->name().compare(typeSize, std::string::npos, elementId, cmpStart) == 0;
    });
    if (element != mElements.end())
    {
        return {*element, element - mElements.begin()};
    }
    return {nullptr, 0};
}

const FhirStructureDefinition* FhirStructureDefinition::parentType() const
{
    const FhirStructureDefinition* parent{};
    if (kind() == Kind::slice)
    {
        parent = mRepositoryBackend->findTypeById(mElements.at(0)->typeId());
        Expect3(parent != nullptr, "base type for slice '" + url() + "' not found: " + typeId(), std::logic_error);
    }
    else
    {
        auto parentKey = mResourceGroup->find(DefinitionKey{baseDefinition()}).first;
        FPExpect3(parentKey.version.has_value(), "basetype for " + urlAndVersion() + " not found: " + baseDefinition(),
                  std::logic_error);
        parent = mRepositoryBackend->findDefinition(parentKey.url, *parentKey.version);
        Expect3(parent != nullptr, "base definition not found for '" + url() + "': " + baseDefinition(),
                std::logic_error);
    }
    return parent;
}

bool FhirStructureDefinition::isDerivedFrom(const fhirtools::FhirStructureDefinition& baseProfile) const
{
    return isDerivedFrom(baseProfile.urlAndVersion());
}

//NOLINTNEXTLINE [misc-no-recursion]
bool FhirStructureDefinition::isDerivedFrom(const std::string_view& baseUrl) const
{
    if (url() == baseUrl || urlAndVersion() == baseUrl)
    {
        return true;
    }
    if (derivation() == Derivation::basetype)
    {
        return false;
    }
    return parentType()->isDerivedFrom(baseUrl);
}

// NOLINTNEXTLINE(misc-no-recursion)
const FhirStructureDefinition* FhirStructureDefinition::baseType() const
{
    if (derivation() != Derivation::constraint)
    {
        return this;
    }
    return parentType()->baseType();
}

const FhirStructureDefinition& FhirStructureDefinition::primitiveToSystemType() const
{
    if (isSystemType())
    {
        return *this;
    }
    Expect3(kind() == FhirStructureDefinition::Kind::primitiveType,
            "systemTypeFor called with structure of kind: " + to_string(kind()), std::logic_error);
    const auto valueElement = findElement(typeId() + ".value");
    Expect3(valueElement != nullptr, "Primitive type has no value element.", std::logic_error);
    const auto* valueType = mRepositoryBackend->findTypeById(valueElement->typeId());
    Expect3(valueType != nullptr, "Type not found: " + valueElement->typeId(), std::logic_error);
    Expect3(valueType->isSystemType(), "Value attribute of " + url() + " is not a system type", std::logic_error);
    return *valueType;
}

const std::shared_ptr<const fhirtools::FhirResourceGroup>& fhirtools::FhirStructureDefinition::resourceGroup() const
{
    return mResourceGroup;
}

const fhirtools::FhirStructureRepositoryBackend& fhirtools::FhirStructureDefinition::repositoryBackend() const
{
    return *mRepositoryBackend;
}


class FhirStructureDefinition::Builder::FhirSlicingBuilder : public FhirSlicing::Builder
{
public:
    using Builder::Builder;
};

FhirStructureDefinition::Builder::~Builder() = default;

FhirStructureDefinition::Builder::Builder()
    : mStructureDefinition(std::make_unique<FhirStructureDefinition>())
{
}

//NOLINTNEXTLINE(hicpp-noexcept-move, performance-noexcept-move-constructor) - construction of FhirStructureDefinition might throw
fhirtools::FhirStructureDefinition::Builder::Builder(Builder&& other) noexcept
    : Builder{}
{
    swap(mFhirSlicingBuilders, other.mFhirSlicingBuilders);
    swap(mStructureDefinition, other.mStructureDefinition);
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::url(std::string url_)
{
    mStructureDefinition->mUrl = std::move(url_);
    return *this;
}

FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::initGroup(const FhirResourceGroupResolver& resolver)
{
    mStructureDefinition->mResourceGroup = resolver.findGroup(*mStructureDefinition);
    return *this;
}

FhirStructureDefinition::Builder&
fhirtools::FhirStructureDefinition::Builder::group(std::shared_ptr<const FhirResourceGroup> group)
{
    mStructureDefinition->mResourceGroup = std::move(group);
    return *this;
}

FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::version(FhirVersion version_)
{
    mStructureDefinition->mVersion = std::move(version_);
    return *this;
}

FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::sourceFile(std::filesystem::path path)
{
    mStructureDefinition->mSourceFile = std::move(path);
    return *this;
}

FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::typeId(std::string type_)
{
    mStructureDefinition->mTypeId = std::move(type_);
    return *this;
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::name(std::string name_)
{
    mStructureDefinition->mName = std::move(name_);
    return *this;
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::baseDefinition(std::string baseDefinition_)
{
    mStructureDefinition->mBaseDefinition = std::move(baseDefinition_);
    return *this;
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::derivation(Derivation derivation_)
{
    mStructureDefinition->mDerivation = derivation_;
    return *this;
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::kind(Kind kind_)
{
    mStructureDefinition->mKind = kind_;
    return *this;
}

FhirStructureDefinition::Builder&
FhirStructureDefinition::Builder::repositoryBackend(gsl::not_null<const FhirStructureRepositoryBackend*> backend)
{
    mStructureDefinition->mRepositoryBackend = backend.get();
    return *this;
}

FhirStructureDefinition::Builder&
FhirStructureDefinition::Builder::addElement(FhirElement::Builder elementBuilder,
                                             std::list<std::string> withTypes)
{
    elementBuilder.structureDefinition(mStructureDefinition.get());
    auto element = elementBuilder.getAndReset();
    if (element->hasBinding() && element->cardinality().max > 0)
    {
        // eld-11: Binding can only be present for coded elements, string, and uri
        // binding.empty() or type.code.empty() or type.select((code = 'code') or (code = 'Coding') or (code='CodeableConcept') or (code = 'Quantity') or (code = 'string') or (code = 'uri')).exists()
        static std::unordered_set<std::string_view> validBindingTypeIds{"code",     "Coding", "CodeableConcept",
                                                                        "Quantity", "string", "uri"};
        bool hasValidBindingType{false};
        std::ostringstream types;
        std::string_view sep{};
        for (const auto& elementType : withTypes)
        {
            types << sep << elementType;
            sep = ", ";
            if (validBindingTypeIds.contains(elementType))
            {
                hasValidBindingType = true;
                break;
            }
        }
        // binding to `http://hl7.org/fhir/ValueSet/duration-units` for type `Duration` is strangely added during snapshot generation
        bool strangeDurationBinding = (withTypes.size() == 1 && withTypes.front() == "Duration" &&
                                       element->binding().key.url == "http://hl7.org/fhir/ValueSet/duration-units");
        if (! hasValidBindingType && ! strangeDurationBinding)
        {
            TLOG(INFO) << "Element " << element->originalName() << " of " << mStructureDefinition->key()
                       << " has defined a binding (" + to_string(element->binding().key) +
                              ") but does not contain a valid binding type: "
                       << types.view();
        }
    }
    const auto originalName = element->originalName();
    bool added = addElementInternal(std::move(element), std::move(withTypes));
    Expect(added, "Failed to add element: " + originalName);
    return *this;
}

//NOLINTNEXTLINE(misc-no-recursion)
bool FhirStructureDefinition::Builder::addElementInternal(std::shared_ptr<const FhirElement> element,
                                                          std::list<std::string> withTypes)
{
    using namespace std::string_view_literals;
    using namespace fhirtools::constants;
    static const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    const auto& elementName = element->name();
    auto placeholderPos =
        std::ranges::search(elementName, typePlaceholder).begin();
    if (placeholderPos == elementName.end())
    {
        Expect3(withTypes.size() <= 1, "More than one element type for element without placeholder: " + elementName,
                std::logic_error);
        return addElementInternal(std::move(element));
    }
    Expect3(not withTypes.empty(), "No type for element with placeholder: " + elementName, std::logic_error);
    std::string_view prefix{elementName.begin(), placeholderPos};
    if (prefix.find(':') != std::string_view::npos)
    {
        return addSliceElement(element, std::move(withTypes));
    }
    std::string_view suffix{placeholderPos + typePlaceholder.size(), elementName.end()};
    if (suffix.empty() || suffix.find('.') == std::string_view::npos)
    {
        bool added = false;
        for (const auto& type : withTypes)
        {
            FPExpect3(type.find_first_of("#.") == std::string::npos, "contentReference not supported with placeholder",
                      std::logic_error);
            Expect3(not type.empty(), "Empty type id: " + elementName, std::logic_error);
            std::string infix = type;
            infix[0] = ctype.toupper(infix[0]);
            std::string name;
            name.reserve(prefix.size() + infix.size() + suffix.size());
            name.append(prefix).append(infix).append(suffix);
            FhirElement::Builder builder{*element};
            builder.name(name).originalName(elementName).typeId(type);
            if (addElementInternal(builder.getAndReset(), {}))
            {
                added = true;
            }
        }
        return added;
    }
    std::string_view baseElementName{elementName.begin(), placeholderPos + typePlaceholder.size()};
    bool added = false;
    const auto& elements = mStructureDefinition->elements();
    size_t elementCount = elements.size();// do not consider elements that where added by the loop
    for (size_t i = 0; i < elementCount; ++i)
    {
        const auto& e = elements[i];
        if (e->originalName() == baseElementName)
        {
            std::string name;
            name.reserve(e->name().size() + suffix.size());
            name.append(e->name()).append(suffix);
            FhirElement::Builder builder{*element};
            builder.name(name).originalName(elementName);
            if (addElementInternal(builder.getAndReset(), withTypes))
            {
                added = true;
            }
        }
    }
    Expect(added, "Failed to add element: " + element->name());

    return added;
}

//NOLINTNEXTLINE(misc-no-recursion)
bool FhirStructureDefinition::Builder::addElementInternal(std::shared_ptr<const FhirElement> element)
{
    using boost::algorithm::starts_with;
    Expect3(mStructureDefinition->findElement(element->name()) == nullptr, "duplicate element id: " + element->name(),
            std::logic_error);

    if (! mStructureDefinition->mElements.empty())
    {
        auto&& prev = mStructureDefinition->mElements.back();
        if (starts_with(element->name(), prev->name() + '.'))
        {
            const auto originalName = prev->originalName();
            //NOLINTNEXTLINE(modernize-loop-convert) -- std::views::reverse fails in clang-tidy, too
            for (auto eIt = mStructureDefinition->mElements.rbegin(), end = mStructureDefinition->mElements.rend();
                 eIt != end; ++eIt)
            {
                auto& e = *eIt;
                if (e->originalName() != originalName)
                {
                    break;
                }
                e = FhirElement::Builder{*e}.isBackbone(true).getAndReset();
            }
        }
        if (element->name().find(':') != std::string::npos &&
            ! element->name().starts_with("http://hl7.org/fhirpath/System."))
        {
            if (element->contentReference().empty())
            {
                FPExpect(! element->typeId().empty(), "element has neither type nor contentReference.");
                return addSliceElement(element, {element->typeId()});
            }
            return addSliceElement(element, {element->contentReference()});
        }
    }

    Expect3(element->name().find(':') == std::string::npos ||
                element->name().starts_with("http://hl7.org/fhirpath/System."),
            "inserted element shouldn't contain colon: " + element->name(), std::logic_error);
    if (element->slicing())
    {
        auto result = mFhirSlicingBuilders.emplace(mStructureDefinition->mElements.size(),
                                                   FhirSlicingBuilder{*element->slicing(), element->name()});
        result.first->second.repositoryBackend(mStructureDefinition->mRepositoryBackend);
    }
    if (element->typeId().empty() && element->contentReference().empty() && ! mStructureDefinition->isSystemType())
    {
        Expect3(mStructureDefinition->kind() == Kind::slice,
                "non-slice elements must have type or contentReference here.", std::logic_error);
        element = FhirElement::Builder{*element}.contentReference('#' + element->name()).getAndReset();
    }
    mStructureDefinition->mElements.emplace_back(std::move(element));
    return true;
}

//NOLINTNEXTLINE(misc-no-recursion)
bool FhirStructureDefinition::Builder::addSliceElement(const std::shared_ptr<const FhirElement>& sliceElement,
                                                       std::list<std::string> withTypes)
{
    const auto& name = sliceElement->name();
    std::string_view namev{name};
    auto colon = namev.find(':');
    Expect(colon != std::string_view::npos, "Missing slice name: " + name);
    auto prefix = namev.substr(0, colon);
    auto dot = namev.find('.', colon);
    auto suffix = dot != std::string_view::npos ? namev.substr(dot + 1) : std::string_view{};
    bool added = false;
    auto& elements = mStructureDefinition->mElements;
    ensureSliceBaseElement(sliceElement);
    for (size_t i = 0; i < elements.size(); ++i)
    {
        auto& element = elements[i];
        if (element->originalName() != prefix && element->name() != prefix)
        {
            continue;
        }
        if (suffix.empty() && std::ranges::none_of(withTypes, [&](const std::string& t) {
                return t == element->typeId() || t == element->contentReference();
            }))
        {
            continue;
        }
        auto& slicingBuilder = getBuilder(i);

        FhirElement::Builder sliceElementBuilder{*sliceElement};
        sliceElementBuilder.originalName(sliceElement->name());
        if (! suffix.empty() || element->typeId().empty())
        {
            if (slicingBuilder.addElement(sliceElementBuilder.getAndReset(), withTypes, *mStructureDefinition))
            {
                added = true;
            }
        }
        else
        {
            if (slicingBuilder.addElement(sliceElementBuilder.getAndReset(), {element->typeId()},
                                          *mStructureDefinition))
            {
                added = true;
            }
        }
    }
    return added;
}

FhirStructureDefinition::Builder::FhirSlicingBuilder& FhirStructureDefinition::Builder::getBuilder(size_t elementIdx)
{
    auto builder = mFhirSlicingBuilders.find(elementIdx);
    if (builder != mFhirSlicingBuilders.end())
    {
        return builder->second;
    }
    const auto& element = mStructureDefinition->mElements.at(elementIdx);
    auto& newBuilder = mFhirSlicingBuilders[elementIdx];
    newBuilder.repositoryBackend(mStructureDefinition->mRepositoryBackend);
    newBuilder.slicePrefix(element->name());
    return newBuilder;
}

void FhirStructureDefinition::Builder::commitSlicing(size_t elementIdx)
{
    auto&& prev = mStructureDefinition->mElements.at(elementIdx);
    prev = FhirElement::Builder{*prev}.slicing(std::move(mFhirSlicingBuilders.at(elementIdx))).getAndReset();
    mFhirSlicingBuilders.erase(elementIdx);
}

//NOLINTNEXTLINE(misc-no-recursion)
bool FhirStructureDefinition::Builder::ensureSliceBaseElement(const std::shared_ptr<const FhirElement>& sliceElement)
{
    auto name = splitSlicedName(sliceElement->name());
    if (name.sliceName.empty())
    {
        return true;
    }
    for (const auto& element : mStructureDefinition->mElements)
    {
        if (name.baseElement == element->name())
        {
            return true;
        }
    }
    Expect(name.suffix.empty(), "Cannot synthesize base element for: " + sliceElement->originalName());
    TLOG(INFO) << "No unsliced element for: " << sliceElement->originalName() << " - synthesizing";
    auto originalName = splitSlicedName(sliceElement->originalName());
    return addElementInternal(FhirElement::Builder{*sliceElement}
                                  .name(std::string{name.baseElement})
                                  .originalName(std::string{originalName.baseElement})
                                  .getAndReset(),
                              {sliceElement->typeId()});
}

FhirStructureDefinition::Builder::SplitSlicedNameResult
FhirStructureDefinition::Builder::splitSlicedName(std::string_view name)
{
    SplitSlicedNameResult result;
    auto colon = name.find(':');
    result.baseElement = name.substr(0, colon);
    if (colon == std::string_view::npos)
    {
        return result;
    }
    auto dot = name.find('.', colon);
    result.sliceName = name.substr(colon + 1, dot - colon - 1);
    if (dot == std::string_view::npos)
    {
        return result;
    }
    result.suffix = name.substr(dot);
    return result;
}

void fhirtools::FhirStructureDefinition::Builder::reset()
{
    mStructureDefinition = std::make_unique<FhirStructureDefinition>();
    mFhirSlicingBuilders.clear();
}

std::unique_ptr<FhirStructureDefinition> FhirStructureDefinition::Builder::getAndReset()
{
    Expect3(mStructureDefinition->mRepositoryBackend != nullptr,  "structureRepository not set.", std::logic_error);
    while (! mFhirSlicingBuilders.empty())
    {
        commitSlicing(mFhirSlicingBuilders.begin()->first);
    }
    auto prev = std::move(mStructureDefinition);
    reset();
    prev->validate();
    return prev;
}
