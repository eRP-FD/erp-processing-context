/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "Collection.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"

using fhirtools::Collection;

Collection::Collection(std::vector<value_type>&& v)
    : std::vector<value_type>(std::move(v))
{
}

Collection::value_type Collection::singleOrEmpty() const
{
    FPExpect(size() <= 1, "Collection size must be 0 or 1");
    if (! empty())
    {
        return at(0);
    }
    return {};
}

Collection::value_type Collection::single() const
{
    FPExpect(size() == 1, "Collection size must be 1");
    return at(0);
}

Collection::value_type Collection::boolean() const
{
    auto singleElement = singleOrEmpty();
    if (singleElement && singleElement->type() == Element::Type::Boolean)
    {
        return singleElement;
    }
    else if (singleElement)
    {
        // http://hl7.org/fhirpath/#singleton-evaluation-of-collections
        // ELSE IF the collection contains a single node AND the expected input type is Boolean THEN
        //  The collection evaluates to true
        return {std::make_shared<PrimitiveElement>(singleElement->getFhirStructureRepository(), Element::Type::Boolean,
                                                   true)};
    }
    return {};
}

bool Collection::contains(const value_type& element) const
{
    return std::ranges::any_of(*this, [&element](const auto& item) {
        if (item->type() == element->type())
        {
            auto res = item->equals(*element);
            return res && *res == true;
        }
        return false;
    });
}

void Collection::append(std::vector<value_type>&& other)
{
    for (auto&& item : other)
    {
        emplace_back(std::move(item));
    }
    other.clear();
}

std::ostream& fhirtools::Collection::json(std::ostream& inStr)
{
    inStr << '[';
    std::string_view sep;
    for (const auto& item : *this)
    {
        inStr << sep;
        sep = ", ";
        item->json(inStr);
    }
    inStr << ']';
    return inStr;
}

std::optional<bool> fhirtools::Collection::equals(const Collection& rhs) const
{
    if (empty() || rhs.empty())
    {
        return {};
    }
    if (size() != rhs.size())
    {
        return false;
    }
    if (size() == 1 && rhs.size() == 1)
    {
        return single()->equals(*rhs.single());
    }
    for (size_t i = 0, end = size(); i < end; ++i)
    {
        const auto& lhs = operator[](i);
        if (isImplicitConvertible(lhs->type(), rhs[i]->type()))
        {
            if (lhs->equals(*rhs[i]) != true)
            {
                return false;
            }
        }
        else if (isImplicitConvertible(rhs[i]->type(), lhs->type()))
        {
            if (rhs[i]->equals(*lhs) != true)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

std::ostream& fhirtools::operator<<(std::ostream& os, const Collection& collection)
{
    os << "[";
    std::string_view sep;
    for (const auto& item : collection)
    {
        os << sep;
        if (item)
        {
            os << *item;
        }
        else
        {
            os << "nullptr";
        }
        sep = ", ";
    }
    os << "]";
    return os;
}
