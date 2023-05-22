/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tpm/PcrSet.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <rapidjson/pointer.h>


PcrSet PcrSet::defaultSet (void)
{
    PcrSet set;
    set.mRegisters.emplace(static_cast<uint8_t>(0));
    set.mRegisters.emplace(static_cast<uint8_t>(3));
    set.mRegisters.emplace(static_cast<uint8_t>(4));
    return set;
}


PcrSet PcrSet::fromString (const std::string& stringValue)
{
    PcrSet pcrSet;

    const auto innerList = String::removeEnclosing("[", "]", stringValue);

    for (const auto& pcrItem : String::split(innerList, ','))
    {
        const uint8_t value = gsl::narrow<uint8_t>(std::stoi(pcrItem));
        Expect(value < 16, "pcr registers must be in range [0,15]");
        pcrSet.mRegisters.emplace(value);
    }

    return pcrSet;
}


PcrSet PcrSet::fromList (const std::vector<size_t>& registers)
{
    PcrSet pcrSet;

    for (const auto pcrItem : registers)
    {
        const uint8_t value = gsl::narrow<uint8_t>(pcrItem);
        Expect(value < 16, "pcr registers must be in range [0,15]");
        pcrSet.mRegisters.emplace(value);
    }

    return pcrSet;
}


std::optional<PcrSet> PcrSet::fromJson (const rapidjson::Document& document, std::string_view pointerToPcrSetArray)
{
    const auto* value = rapidjson::Pointer(rapidjson::StringRef(pointerToPcrSetArray.data(), pointerToPcrSetArray.size())).Get(document);
    if (value == nullptr)
        return {};

    Expect(value->IsArray(), "pcr set JSON value is not an array");
    const auto array = value->GetArray();
    PcrSet result;
    for (const rapidjson::Value& item : array)
    {
        Expect(item.IsInt(), "array item is not an integer");
        result.mRegisters.emplace(gsl::narrow<uint8_t>(item.GetInt()));
    }
    return result;
}


std::vector<size_t> PcrSet::toPcrList (void) const
{
    std::vector<size_t> list;
    list.reserve(mRegisters.size());
    std::copy(mRegisters.begin(), mRegisters.end(), std::back_inserter(list));
    return list;
}


std::string PcrSet::toString (const bool addBrackets) const
{
    std::ostringstream out;

    if (addBrackets)
        out << '[';
    bool first = true;
    for (const auto value : mRegisters)
    {
        if (first)
            first = false;
        else
            out << ',';
        out << static_cast<size_t>(value);
    }
    if (addBrackets)
        out << ']';

    return out.str();
}


bool PcrSet::operator== (const PcrSet& other) const
{
    return mRegisters == other.mRegisters;
}


size_t PcrSet::size (void) const
{
    return mRegisters.size();
}


PcrSet::const_iterator PcrSet::begin (void) const
{
    return mRegisters.begin();
}


PcrSet::const_iterator PcrSet::end (void) const
{
    return mRegisters.end();
}
