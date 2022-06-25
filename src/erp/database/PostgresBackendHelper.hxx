/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_POSTGRESBACKENDHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_POSTGRESBACKENDHELPER_HXX

#include <pqxx/transaction>
#include <cstdint>
#include <optional>
#include <string_view>

namespace db_model
{
class Blob;
}
class UrlArguments;

struct QueryDefinition {
    const char* name{};
    std::string query;
};

namespace PostgresBackendHelper
{
[[nodiscard]] uint64_t executeCountQuery(pqxx::work& transaction, const std::string_view& query,
                                         const db_model::Blob& paramValue, const std::optional<UrlArguments>& search,
                                         const std::string_view& context);
}


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_POSTGRESBACKENDHELPER_HXX
