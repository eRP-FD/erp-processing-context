/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#include "erp/model/Kvnr.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Random.hxx"
#include "erp/util/String.hxx"

#include <regex>
#include <sstream>

namespace model
{

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
    // We cannot use the pkv naming system in case we are rendering for 2022 profiles.
    // Even though the pkv profiles are only valid starting with 2023 profiles,
    // this helps for compatibility with our tests
    if (namingSystem == resource::naming_system::gkvKvid10 ||
        namingSystem == resource::naming_system::deprecated::gkvKvid10)
    {
        mType = Type::gkv;
    }
    else if (namingSystem == resource::naming_system::pkvKvid10)
    {
        mType = Type::pkv;
    }
    else
    {
        ModelFail("Unknown naming system for kvnr: " + std::string{namingSystem});
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

std::string_view Kvnr::namingSystem(bool deprecated) const
{
    if (! deprecated && mType == Type::pkv)
    {
        return resource::naming_system::pkvKvid10;
    }
    else if (deprecated)
    {
        return resource::naming_system::deprecated::gkvKvid10;
    }
    else
    {
        return resource::naming_system::gkvKvid10;
    }
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

bool Kvnr::valid() const
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
    }
    catch (const std::out_of_range&)
    {
    }
    A_20751.finish();
    return false;
}

bool Kvnr::isKvnr(const std::string& value)
{
    static const std::regex matchKvnr("[A-Z][0-9]{9}");
    return std::regex_match(value, matchKvnr);
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
