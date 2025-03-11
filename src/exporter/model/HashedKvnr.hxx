/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_HASHEDKVNR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_HASHEDKVNR_HXX

#include <string>
#include <string_view>


namespace db_model
{
class HashedKvnr;
}

class JsonLog;

namespace model
{

class HashedKvnr
{
public:
    HashedKvnr(std::string_view hashedKvnr);
    HashedKvnr(const db_model::HashedKvnr& hashedKvnr);
    std::string getLoggingId() const;

private:
    std::string mHashedKvnr;
};

JsonLog& operator<<(JsonLog& log, const HashedKvnr& hashedKvnr);
JsonLog&& operator<<(JsonLog&& log, const HashedKvnr& hashedKvnr);

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_HASHEDKVNR_HXX
