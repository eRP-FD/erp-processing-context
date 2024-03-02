/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Lanr.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/Mod10.hxx"
#include "erp/util/String.hxx"

#include <regex>
#include <sstream>
#include <glog/logging.h>

namespace model
{

Lanr::Lanr(std::string lanr, Type type)
    : mValue{std::move(lanr)}
    , mType{type}
{
}

Lanr::Lanr(std::string_view lanr, Type type)
    : mValue{lanr}
    , mType{type}
{
}

Lanr::Lanr(const char* lanr, Type type)
    : mValue{lanr}
    , mType{type}
{
}

const std::string& Lanr::id() const&
{
    return mValue;
}

void Lanr::setId(std::string_view id)
{
    mValue = id;
}

std::string Lanr::id() &&
{
    return std::move(mValue);
}

Lanr::Type Lanr::getType() const
{
    return mType;
}

std::string_view Lanr::namingSystem(bool deprecated) const
{
    switch (mType)
    {
        case Type::lanr:
            return resource::naming_system::kbvAnr;
        case Type::zanr:
            return deprecated ? resource::naming_system::deprecated::kbvZanr : resource::naming_system::kbvZanr;
        case Type::unspecified:
            break;
    }
    ModelFail("No naming system for unspecified ANR type");
}

bool Lanr::validFormat() const
{
    return isLanr(mValue);
}

bool Lanr::isLanr(const std::string& value)
{
    static const std::regex matchLanr("[0-9]{9}");
    return std::regex_match(value, matchLanr);
}

bool Lanr::isPseudoLanr() const
{
    static const auto exceptions = {"555555"};
    for (const auto* head : exceptions)
    {
        if (String::starts_with(mValue, head))
            return true;
    }
    return false;
}

bool Lanr::validChecksum() const
{
    if (! validFormat())
        return false;
    // skip the last two group code characters
    auto numericPart = std::string_view{mValue.data(), 6};
    auto checksum = checksum::mod10<4, 9, false>(numericPart) - '0';
    // The checksum value from the string is the difference to 10 of the mod10 value
    // if the difference is 10, the checksum value is 0.
    return ((checksum + mValue[6] - '0') % 10 == 0) || isPseudoLanr();
}

bool operator==(const Lanr& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

std::ostream& operator<<(std::ostream& os, const Lanr& lanr)
{
    os << lanr.id();
    return os;
}

}// namespace model
