/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */


#include "erp/model/TelematikId.hxx"

namespace model {

TelematikId::TelematikId(std::string_view telematikId)
    : mId{telematikId}
{
}

bool TelematikId::valid() const
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

bool operator==(const TelematikId& lhs, const TelematikId& rhs)
{
    return lhs.id() == rhs.id();
}

bool operator==(const TelematikId& lhs, std::string_view rhs)
{
    return lhs.id() == rhs;
}

} // namespace model