/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/model/TelematikId.hxx"

namespace model {

TelematikId::TelematikId(std::string_view telematikId)
    : mId{telematikId}
{
}

bool TelematikId::validFormat() const
{
    return isTelematikId(mId);
}

const std::string& TelematikId::id() const&
{
    return mId;
}

std::string TelematikId::id() &&
{
    return std::move(mId);
}

bool TelematikId::isTelematikId(std::string_view id)
{
    return id.find_first_of('-') != std::string_view::npos;
}

bool TelematikId::isOeffentlicheApotheke() const
{
    return mId.starts_with("3-") || mId.starts_with("9-");
}

bool TelematikId::isKrankenhausApotheke() const
{
    return mId.starts_with("5-");
}

bool TelematikId::isKostentraeger() const
{
    return mId.starts_with("8-");
}

bool operator==(const TelematikId& lhs, const TelematikId& rhs)
{
    return lhs.id() == rhs.id();
}

bool operator==(const TelematikId& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

} // namespace model