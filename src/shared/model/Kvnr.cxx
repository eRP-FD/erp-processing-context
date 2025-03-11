/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Kvnr.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Mod10.hxx"
#include "shared/util/String.hxx"

#include <regex>
#include <sstream>

namespace model
{

namespace
{
// convert a given character to the numeric value of the alphabet
// A -> "1", .., Z -> "26"
std::string charToAlphabetNumber(char c)
{
    int idx = c - 'A' + 1;
    return std::to_string(idx);
}
} // namespace

Kvnr::Kvnr(std::string kvnr, Type type)
    : mValue{std::move(kvnr)}
    , mType{type}
{
}

Kvnr::Kvnr(std::string_view kvnr, Type type)
    : mValue{kvnr}
    , mType{type}
{
}

Kvnr::Kvnr(const char* kvnr, Type type)
    : mValue{kvnr}
    , mType{type}
{
}

Kvnr::Kvnr(std::string_view kvnr, std::string_view namingSystem)
    : mValue{kvnr}
{
    if (namingSystem == resource::naming_system::gkvKvid10)
    {
        mType = Type::gkv;
    }
    else if (namingSystem == resource::naming_system::pkvKvid10)
    {
        mType = Type::pkv;
    }
    else
    {
        mType = Type::unspecified;
    }
}

const std::string& Kvnr::id() const&
{
    return mValue;
}

void Kvnr::setId(std::string_view id)
{
    mValue = id;
}

std::string_view Kvnr::namingSystem() const
{
    switch (mType)
    {
        case Type::pkv:
            return resource::naming_system::pkvKvid10;
        case Type::gkv:
        case Type::unspecified:// not super important - see ERP-19763
            return resource::naming_system::gkvKvid10;
    }
    Fail2("Invalid value for mType: " + std::to_string(static_cast<uintmax_t>(mType)), std::logic_error);
}

std::string Kvnr::id() &&
{
    return std::move(mValue);
}

Kvnr::Type Kvnr::getType() const
{
    return mType;
}

void Kvnr::setType(Type type)
{
    mType = type;
}

bool Kvnr::validFormat() const
{
    return isKvnr(mValue);
}

bool Kvnr::verificationIdentity() const
{
    A_20751.start("Recognize the verification identity");
    if (! isKvnr(mValue) || ! String::starts_with(mValue, "X0000"))
    {
        return false;
    }
    const std::string subset = mValue.substr(5, 4);
    try
    {
        int value = std::stoi(subset);
        if (value >= 1 && value <= 5000)
        {
            return true;
        }
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
    A_20751.finish();
    return false;
}

bool Kvnr::isKvnr(const std::string& value)
{
    static const std::regex matchKvnr("[A-Z][0-9]{9}");
    return std::regex_match(value, matchKvnr);
}

bool Kvnr::validChecksum() const
{
    if (! validFormat())
    {
        return false;
    }
    // before calculating the checksum, we need to convert the
    // letter to the numeric
    auto numericPart = std::string_view{mValue.data() + 1, 8};
    const auto checksumValue = checksum::mod10<1, 2>(charToAlphabetNumber(mValue.front()).append(numericPart));
    return checksumValue == mValue.back();
}

bool operator==(const Kvnr& lhs, const Kvnr& rhs)
{
    return lhs.id() == rhs.id();
}

bool operator==(const Kvnr& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

std::ostream& operator<<(std::ostream& os, const Kvnr& kvnr)
{
    os << kvnr.id();
    return os;
}

}// namespace model
