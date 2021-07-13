#include "FhirElement.hxx"

#include <iostream>

namespace repesentation_strings
{
    static const std::string xmlAttr  = "xmlAttr";
    static const std::string xmlText  = "xmlText";
    static const std::string typeAttr = "typeAttr";
    static const std::string cdaText  = "cdaText";
    static const std::string xhtml    = "xhtml";
    static const std::string element  = "element";
}


FhirElement::FhirElement() = default;


FhirElement::~FhirElement() = default;

FhirElement::Builder::Builder()
    : mFhirElement(std::make_unique<FhirElement>())
{
}

FhirElement::Builder::Builder(const FhirElement& elementTemplate)
    : mFhirElement(std::make_unique<FhirElement>(elementTemplate))
{
}


FhirElement FhirElement::Builder::getAndReset()
{
    FhirElement prev{std::move(*mFhirElement)};
    mFhirElement = std::make_unique<FhirElement>();
    return prev;
}


std::ostream& operator << (std::ostream& out, const FhirElement& element)
{
    std::ostream xout(out.rdbuf());
    xout.setf(std::ios::boolalpha);
    // clang-format off
    xout << R"({ "name": ")" << element.name()
         << R"(", "typeId": ")" << element.typeId()
         << R"(", "representation": ")" << element.representation()
         << R"(", "isArray": )" << element.isArray() << " }";
    // clang-format on
    return out;
}

const std::string& to_string(FhirElement::Representation representation)
{
    using Representation = FhirElement::Representation;
    switch (representation)
    {
        // clang-format off
        case Representation::xmlAttr:  return repesentation_strings::xmlAttr;
        case Representation::xmlText:  return repesentation_strings::xmlText;
        case Representation::typeAttr: return repesentation_strings::typeAttr;
        case Representation::cdaText:  return repesentation_strings::cdaText;
        case Representation::xhtml:    return repesentation_strings::xhtml;
        case Representation::element:  return repesentation_strings::element;
        // clang-format on
    }
    throw std::logic_error("Invalid value for FhirElement::Representation: " + std::to_string(static_cast<uintmax_t>(representation)));
}

std::ostream& operator << (std::ostream& out, FhirElement::Representation representation)
{
    return (out << to_string(representation));
}

FhirElement::Representation stringToRepresentation(const std::string_view& str)
{
    using namespace std::string_literals;
    using Representation = FhirElement::Representation;
    if (str == repesentation_strings::xmlAttr)
        return Representation::xmlAttr;
    else if (str == repesentation_strings::xmlText)
        return Representation::xmlText;
    else if (str == repesentation_strings::typeAttr)
        return Representation::typeAttr;
    else if (str == repesentation_strings::cdaText)
        return Representation::cdaText;
    else if (str == repesentation_strings::xhtml)
        return Representation::xhtml;
    else if (str == repesentation_strings::element)
        return Representation::element;
    else
        throw std::logic_error("Cannot convert string to FhirElement::Representation: "s.append(str));
}


FhirElement::Builder& FhirElement::Builder::name(std::string name_)
{
    mFhirElement->mName = std::move(name_);
    return *this;
}


FhirElement::Builder& FhirElement::Builder::typeId(std::string type_)
{
    mFhirElement->mTypeId = std::move(type_);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::contentReference(std::string contentReference_)
{
    mFhirElement->mContentReference = std::move(contentReference_);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::representation(Representation representation_)
{
    mFhirElement->mRepresentation = representation_;
    return *this;
}


FhirElement::Builder& FhirElement::Builder::isArray(bool isArray_)
{
    mFhirElement->mIsArray = isArray_;
    return *this;
}
