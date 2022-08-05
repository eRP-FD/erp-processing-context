/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "FhirElement.hxx"

#include <boost/algorithm/string/split.hpp>
#include <magic_enum.hpp>
#include <sstream>
#include <utility>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

using fhirtools::FhirElement;

namespace repesentation_strings
{
static const std::string xmlAttr = "xmlAttr";
static const std::string xmlText = "xmlText";
static const std::string typeAttr = "typeAttr";
static const std::string cdaText = "cdaText";
static const std::string xhtml = "xhtml";
static const std::string element = "element";
}


FhirElement::FhirElement() = default;


FhirElement::~FhirElement() = default;

std::string_view fhirtools::FhirElement::fieldName() const
{
    size_t dot = mName.rfind('.');
    if (dot == std::string::npos)
    {
        return mName;
    }
    return std::string_view{mName.data() + dot + 1, mName.data() + mName.size()};
}

std::string_view fhirtools::FhirElement::originalFieldName() const
{
    if (mOriginalName.empty())
    {
        return fieldName();
    }
    size_t dot = mOriginalName.rfind('.');
    size_t colon = mOriginalName.rfind(':');
    if (dot == std::string::npos)
    {
        if (colon == std::string::npos)
        {
            // no dot no slice
            return mOriginalName;
        }
        // no dot only slice name.
        return std::string_view{mOriginalName.data(), mOriginalName.data() + colon};
    }
    if (colon == std::string::npos || colon < dot)
    {
        // no slice or slice name is before fieldname
        return std::string_view{mOriginalName.data() + dot + 1, mOriginalName.data() + mOriginalName.size()};
    }
    // slice name after field name
    return std::string_view{mOriginalName.data() + dot + 1, mOriginalName.data() + colon};
}

const std::optional<fhirtools::FhirSlicing>& FhirElement::slicing() const
{
    return mSlicing;
}

FhirElement::Builder::Builder()
    : mFhirElement(std::make_unique<FhirElement>())
{
}

FhirElement::Builder::Builder(FhirElement elementTemplate)
    : mFhirElement(std::make_unique<FhirElement>(std::move(elementTemplate)))
{
}

std::shared_ptr<const FhirElement> FhirElement::Builder::getAndReset()
{
    auto prev{std::move(mFhirElement)};
    mFhirElement = std::make_shared<FhirElement>();
    return prev;
}


std::ostream& fhirtools::operator<<(std::ostream& out, const FhirElement& element)
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

const std::string& fhirtools::to_string(FhirElement::Representation representation)
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
    throw std::logic_error("Invalid value for FhirElement::Representation: " +
                           std::to_string(static_cast<uintmax_t>(representation)));
}

std::ostream& fhirtools::operator<<(std::ostream& out, FhirElement::Representation representation)
{
    return (out << to_string(representation));
}

FhirElement::Representation fhirtools::stringToRepresentation(const std::string_view& str)
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

FhirElement::Builder& FhirElement::Builder::originalName(std::string orig)
{
    mFhirElement->mOriginalName = std::move(orig);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::typeId(std::string type_)
{
    mFhirElement->mTypeId = std::move(type_);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::addProfile(std::string profile)
{
    mFhirElement->mProfiles.emplace_back(std::move(profile));
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


FhirElement::Builder& FhirElement::Builder::isBackbone(bool isBackbone_)
{
    mFhirElement->mIsBackbone = isBackbone_;
    return *this;
}


FhirElement::Builder& FhirElement::Builder::addConstraint(FhirConstraint key)
{
    mFhirElement->mConstraints.emplace_back(std::move(key));
    return *this;
}

FhirElement::Builder& FhirElement::Builder::slicing(FhirSlicing&& slicing)
{
    mFhirElement->mSlicing.emplace(std::move(slicing));
    return *this;
}


FhirElement::Builder& FhirElement::Builder::pattern(std::shared_ptr<const FhirValue> value)
{
    mFhirElement->mPattern = std::move(value);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::fixed(std::shared_ptr<const FhirValue> value)
{
    mFhirElement->mFixed = std::move(value);
    return *this;
}

FhirElement::Builder& FhirElement::Builder::cardinalityMin(uint32_t val)
{
    mFhirElement->mCardinality.min = val;
    return *this;
}

FhirElement::Builder& FhirElement::Builder::cardinalityMax(std::optional<uint32_t> val)
{
    mFhirElement->mCardinality.max = val;
    return *this;
}

FhirElement::Builder& fhirtools::FhirElement::Builder::bindingStrength(const std::string& strength)
{
    if (! mFhirElement->mBinding)
    {
        mFhirElement->mBinding.emplace();
    }
    auto bindingStrength = magic_enum::enum_cast<BindingStrength>(strength);
    FPExpect(bindingStrength, "invalid binding strength: " + strength);
    mFhirElement->mBinding->strength = *bindingStrength;
    return *this;
}

FhirElement::Builder& fhirtools::FhirElement::Builder::bindingValueSet(const std::string& canonical)
{
    if (! mFhirElement->mBinding)
    {
        mFhirElement->mBinding.emplace();
    }
    std::vector<std::string> parts;
    boost::split(parts, canonical, [](char c) {
        return c == '|';
    });
    mFhirElement->mBinding->valueSetUrl = parts[0];
    if (parts.size() > 1)
    {
        mFhirElement->mBinding->valueSetVersion = Version{parts[1]};
    }
    TVLOG(2) << "New Binding to " << canonical;
    return *this;
}

bool FhirElement::Cardinality::isArrayConstraint() const
{
    return min != 0 || max.has_value();
}

bool fhirtools::FhirElement::Cardinality::isFieldConstraint() const
{
    return min != 0 || (max.has_value() && *max == 0);
}

bool fhirtools::FhirElement::Cardinality::isConstraint(bool isArray)
{
    return (!isArray && isFieldConstraint()) || (isArray && isArrayConstraint());
}


fhirtools::ValidationResultList FhirElement::Cardinality::check(uint32_t count, std::string_view elementFullPath,
                                                                const FhirStructureDefinition* profile) const
{
    using namespace std::string_literals;
    using std::to_string;
    ValidationResultList result;
    if (min > count)
    {
        if (min == 1)
        {
            result.add(Severity::error, "missing mandatory element", std::string{elementFullPath}, profile);
        }
        else
        {
            result.add(Severity::error,
                       "At least " + to_string(min) + " elements required, but got only " + to_string(count),
                       std::string{elementFullPath}, profile);
        }
    }
    else if (max && count > max)
    {
        result.add(Severity::error, "At most " + to_string(*max) + " elements expected, but got " + to_string(count),
                   std::string{elementFullPath}, profile);
    }
    else
    {
        result.add(Severity::debug, "cardinality check ok: " + to_string(count) + " is in " + to_string(*this),
                   std::string{elementFullPath}, profile);
    }
    return result;
}

bool fhirtools::FhirElement::Cardinality::restricts(const fhirtools::FhirElement::Cardinality& rhs) const
{
    return min > rhs.min || (max && rhs.max && max < rhs.max) || (max && ! rhs.max);
}


std::ostream& fhirtools::operator<<(std::ostream& out, const FhirElement::Cardinality& cardinality)
{
    using std::to_string;
    out << '[' << cardinality.min << ".." << (cardinality.max ? to_string(*cardinality.max) : "*") << ']';
    return out;
}

std::string fhirtools::to_string(const FhirElement::Cardinality& cardinality)
{
    std::ostringstream strm;
    strm << cardinality;
    return strm.str();
}
