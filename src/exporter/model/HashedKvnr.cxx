/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/HashedKvnr.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"

namespace model
{


HashedKvnr::HashedKvnr(std::string_view hashedKvnr)
    : mHashedKvnr(hashedKvnr)
{
}
HashedKvnr::HashedKvnr(const db_model::HashedKvnr& hashedKvnr)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    : HashedKvnr(std::string_view(reinterpret_cast<const char*>(hashedKvnr.binarystring().data()),
                                  hashedKvnr.binarystring().size()))
{
}

std::string HashedKvnr::getLoggingId() const
{
    return String::toHexString(mHashedKvnr);
}

JsonLog& operator<<(JsonLog& log, const HashedKvnr& hashedKvnr)
{
    log << KeyValue("kvnr", hashedKvnr.getLoggingId());
    return log;
}


JsonLog&& operator<<(JsonLog&& log, const HashedKvnr& hashedKvnr)
{
    log << KeyValue("kvnr", hashedKvnr.getLoggingId());
    return std::move(log);
}

}
