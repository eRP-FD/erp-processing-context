/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "FhirStructureDefinition.hxx"

#include "erp/fhir/FhirStructureRepository.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/util/Expect.hxx"

#include <boost/algorithm/string.hpp>

using namespace std::string_literals;

namespace kind_string
{
    // clang-format off
    static const std::string primitiveType = "primitive-type";
    static const std::string complexType   = "complex-type";
    static const std::string resource      = "resource";
    static const std::string logical       = "logical";
    static const std::string systemBoolean = "systemBoolean";
    static const std::string systemString  = "systemString";
    static const std::string systemDouble  = "systemDouble";
    static const std::string systemInteger = "systemInteger";
    // clang-format on
}

namespace derivation_string
{
    // clang-format off
    static const std::string basetype       = "basetype";
    static const std::string specialization = "specialization";
    static const std::string constraint     = "constraint";
    // clang-format on
}

FhirStructureDefinition::FhirStructureDefinition() = default;
FhirStructureDefinition::~FhirStructureDefinition() = default;


void FhirStructureDefinition::validate()
{
    Expect(not mUrl.empty(), "Missing url in FhirStructureDefinition.");
    Expect(not mTypeId.empty(), "Missing type in FhirStructureDefinition: " + mUrl);
}

const std::string& to_string(FhirStructureDefinition::Derivation derivation)
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
    throw std::logic_error("Invalid value for Derivation :" + to_string(static_cast<intmax_t>(derivation)));
}

FhirStructureDefinition::Derivation stringToDerivation(const std::string_view& str)
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
        throw std::logic_error("Cannot convert string to FhirStructureDefinition::Derivation: "s.append(str));
}

std::ostream& operator << (std::ostream& out, FhirStructureDefinition::Derivation derivation)
{
    out << to_string(derivation);
    return out;
}

std::ostream& operator << (std::ostream& out, const FhirStructureDefinition& structure)
{
    std::ostream xout(out.rdbuf());

    xout << R"({ "typeId": ")" << structure.typeId();
    xout << R"(", "url": ")" << structure.url();
    xout << R"(", "baseDefinition": ")" << structure.baseDefinition();
    xout << R"(", "derivation": ")" << structure.derivation();
    xout << R"(", "kind": ")" << structure.kind();
    xout << R"(", "elements": [)";
    std::string_view seperator;
    for (const auto& element: structure.elements())
    {
        xout << seperator << element;
        seperator = ", ";
    }
    xout << "] }";
    return out;
}


const std::string& to_string(FhirStructureDefinition::Kind kind)
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
    }
    using std::to_string;
    throw std::logic_error("Invalid value for FhirStructureDefinition::Kind: " + to_string(static_cast<uintmax_t>(kind)));
    // clang-format on
}

FhirStructureDefinition::Kind stringToKind(const std::string_view& str)
{
    using namespace kind_string;
    using Kind = FhirStructureDefinition::Kind;
    if (str == primitiveType)
        return Kind::primitiveType;
    else if (str == complexType)
        return Kind::complexType;
    else if (str == resource)
        return Kind::resource;
    else if (str == logical)
        return Kind::logical;
    else if (str == systemBoolean)
        return Kind::systemBoolean;
    else if (str == systemString)
        return Kind::systemString;
    else if (str == systemDouble)
        return Kind::systemDouble;
    else if (str == systemInteger)
        return Kind::systemInteger;
    else
        throw std::logic_error("Cannot convert string to FhirStructureDefinition::Kind: "s.append(str));
}

bool FhirStructureDefinition::isSystemType() const
{
    switch (mKind) {
         case Kind::primitiveType:
         case Kind::complexType:
         case Kind::resource:
         case Kind::logical:
             return false;
         case Kind::systemBoolean:
         case Kind::systemString:
         case Kind::systemDouble:
         case Kind::systemInteger:
             return true;
    }
    using std::to_string;
    throw std::logic_error("Ivalid value for FhirStructureDefinition::Kind: " + to_string(static_cast<uintmax_t>(mKind)));
}

std::ostream& operator << (std::ostream& out, FhirStructureDefinition::Kind kind)
{
    return (out << to_string(kind));
}

std::shared_ptr<const FhirElement> FhirStructureDefinition::findElement(const std::string& elementId) const
{
    auto element = std::find_if(mElements.begin(), mElements.end(),
                [elementId](const std::shared_ptr<const FhirElement>& e)
                {
                    Expect3(e != nullptr, "element is null.", std::logic_error);
                    return e->name() == elementId;
                });
    if (element != mElements.end())
    {
        return *element;
    }
    return nullptr;
}


//NOLINTNEXTLINE [misc-no-recursion]
bool FhirStructureDefinition::isDerivedFrom(const FhirStructureRepository& repo, const std::string_view& baseUrl) const
{
    if (url() == baseUrl)
    {
        return true;
    }
    if (derivation() == Derivation::basetype)
    {
        return false;
    }
    const auto* baseType = repo.findDefinitionByUrl(baseDefinition());
    Expect3(baseType != nullptr, "base definition not found for '" + url() + "': " + baseDefinition(), std::logic_error);
    return baseType->isDerivedFrom(repo, baseUrl);
}


FhirStructureDefinition::Builder::Builder()
    : mStructureDefinition(std::make_unique<FhirStructureDefinition>())
{
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::url(std::string url_)
{
    mStructureDefinition->mUrl = std::move(url_);
    return *this;
}


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::typeId(std::string type_)
{
    mStructureDefinition->mTypeId = std::move(type_);
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


FhirStructureDefinition::Builder& FhirStructureDefinition::Builder::addElement(std::shared_ptr<const FhirElement> element)
{
    using boost::algorithm::starts_with;
    Expect3(element != nullptr, "element must not be null.", std::logic_error);
    Expect3(mStructureDefinition->findElement(element->name()) == nullptr,
        "duplicate element id: " + element->name(), std::logic_error);

    if (!mStructureDefinition->mElements.empty())
    {
        auto& prev = mStructureDefinition->mElements.back();
        if (starts_with(element->name(), prev->name() + '.'))
        {
            prev = FhirElement::Builder{*prev}.isBackbone(true).getAndReset();
        }
    }

    mStructureDefinition->mElements.emplace_back(std::move(element));
    return *this;
}


FhirStructureDefinition FhirStructureDefinition::Builder::getAndReset()
{
    auto prev = std::move(*mStructureDefinition);
    mStructureDefinition = std::make_unique<FhirStructureDefinition>();
    prev.validate();
    return prev;
}
