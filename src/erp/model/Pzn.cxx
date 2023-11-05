/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Pzn.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/String.hxx"

#include <regex>
#include <sstream>

namespace model
{

Pzn::Pzn(std::string pzn)
    : mValue{std::move(pzn)}
{
}

Pzn::Pzn(std::string_view pzn)
    : mValue{pzn}
{
}

Pzn::Pzn(const char* pzn)
    : mValue{pzn}
{
}

const std::string& Pzn::id() const&
{
    return mValue;
}

void Pzn::setId(std::string_view id)
{
    mValue = id;
}

std::string Pzn::id() &&
{
    return std::move(mValue);
}

bool Pzn::validFormat() const
{
    return isPzn(mValue);
}

bool Pzn::isPzn(const std::string& value)
{
    static const std::regex matchPzn("[0-9]{8}");
    return std::regex_match(value, matchPzn);
}

bool Pzn::validChecksum() const
{
    if (! validFormat())
        return false;
    // skip the last two group code characters
    auto numericPart = std::string_view{mValue.data(), 7};
    size_t checksum = 0;
    for (size_t i = 0; i < numericPart.size(); ++i)
    {
        checksum += (i + 1) * static_cast<size_t>(numericPart[i] - '0');
    }
    checksum %= 11;
    return static_cast<char>(checksum) == mValue.back() - '0';
}

bool operator==(const Pzn& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

std::ostream& operator<<(std::ostream& os, const Pzn& pzn)
{
    os << pzn.id();
    return os;
}

}// namespace model
