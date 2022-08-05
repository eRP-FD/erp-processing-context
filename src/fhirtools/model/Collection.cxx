/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
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
        return *item == *element;
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


bool Collection::operator==(const Collection& rhs) const
{
    if (size() != rhs.size())
    {
        return false;
    }
    for (size_t i = 0, end = size(); i < end; ++i)
    {
        const auto& lhs = operator[](i);
        if (isImplicitConvertible(lhs->type(), rhs[i]->type()))
        {
            if (*lhs != *rhs[i])
            {
                return false;
            }
        }
        else if (isImplicitConvertible(rhs[i]->type(), lhs->type()))
        {
            if (*rhs[i] != *lhs)
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

bool Collection::operator!=(const Collection& rhs) const
{
    return ! operator==(rhs);
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
