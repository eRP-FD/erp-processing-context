// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#include "RapidjsonErrorDocument.hxx"

#include <set>


RapidjsonErrorDocument::RapidjsonErrorDocument(ValueType& value)
{
    if (value.IsObject())
    {
        iterateMember(value);
    }
}

const std::optional<RapidjsonErrorDocument::ValueType>& RapidjsonErrorDocument::firstError() const
{
    return firstError_;
}

bool RapidjsonErrorDocument::iterateMember(ValueType& mem)// NOLINT(misc-no-recursion)
{
    static const std::set<std::string> errorIndicators{"type",
                                                       "enum",
                                                       "multipleOf",
                                                       "maximum",
                                                       "minimum",
                                                       "maxLength",
                                                       "minLength",
                                                       "pattern",
                                                       "additionalItems",
                                                       "maxItems",
                                                       "minItems",
                                                       "uniqueItems",
                                                       "maxProperties",
                                                       "minProperties",
                                                       "required",
                                                       "additionalProperties",
                                                       "dependencies",
                                                       "not"};
    if (! mem.IsObject())
    {
        return true;
    }
    for (auto it = mem.MemberBegin(); it != mem.MemberEnd(); ++it)
    {
        if (it->value.IsObject())
        {
            if (errorIndicators.contains(std::string{it->name.GetString()}))
            {
                firstError_.emplace(mem.GetObject());
                return false;
            }
            if (! iterateMember(it->value))
            {
                return false;
            }
        }
        if (it->value.IsArray())
        {
            if (! iterateArray(it->value))
            {
                return false;
            }
        }
    }
    return true;
}

bool RapidjsonErrorDocument::iterateArray(ValueType& arr)// NOLINT(misc-no-recursion)
{
    for (auto* itArr = arr.Begin(); itArr && itArr != arr.End(); ++itArr)
    {
        if (itArr->IsObject())
        {
            if (! iterateMember(*itArr))
            {
                return false;
            }
        }
    }
    return true;
}
