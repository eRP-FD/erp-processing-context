/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Iknr.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Mod10.hxx"

#include <regex>
#include <sstream>

namespace model
{

Iknr::Iknr(std::string iknr)
    : mValue{std::move(iknr)}
{
}

Iknr::Iknr(std::string_view iknr)
    : mValue{iknr}
{
}

Iknr::Iknr(const char* iknr)
    : mValue{iknr}
{
}

const std::string& Iknr::id() const&
{
    return mValue;
}

void Iknr::setId(std::string_view id)
{
    mValue = id;
}

std::string_view Iknr::namingSystem(bool deprecated) const
{
    if (deprecated)
    {
        return resource::naming_system::deprecated::argeIknr;
    }
    else
    {
        return resource::naming_system::argeIknr;
    }
}

std::string Iknr::id() &&
{
    return std::move(mValue);
}

bool Iknr::validFormat() const
{
    return isIknr(mValue);
}

bool Iknr::isIknr(const std::string& value)
{
    static const std::regex matchIknr("[0-9]{9}");
    return std::regex_match(value, matchIknr);
}

bool Iknr::validChecksum() const
{
    if (! validFormat())
        return false;
    // skip the first two classification characters
    auto numericPart = std::string_view{mValue.data() + 2, 6};
    return checksum::mod10<2, 1>(numericPart) == mValue.back();
}

bool operator==(const Iknr& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

std::ostream& operator<<(std::ostream& os, const Iknr& iknr)
{
    os << iknr.id();
    return os;
}

}// namespace model
