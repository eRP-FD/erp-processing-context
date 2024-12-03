/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_DATABASE_COMMONDATABASEFRONTEND_HXX
#define ERP_PROCESSING_CONTEXT_DATABASE_COMMONDATABASEFRONTEND_HXX

#include "DatabaseCodec.hxx"
#include "DatabaseConnectionInfo.hxx"
#include "erp/database/ErpDatabaseBackend.hxx"
#include "shared/compression/Compression.hxx"
#include "shared/database/DatabaseBackend.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/model/AuditData.hxx"
#include "shared/util/SafeString.hxx"

#include <memory>

namespace db_model
{
class HashedKvnr;
};// namespace db_model

class CommonDatabaseFrontend
{
public:
    CommonDatabaseFrontend(HsmPool& hsmPool, KeyDerivation& keyDerivation);

    static std::shared_ptr<Compression> compressionInstance();

    std::tuple<SafeString, BlobId> auditEventKey(DatabaseBackend& backend,
                                                 const db_model::HashedKvnr& hashedKvnr);

    std::string storeAuditEventData(DatabaseBackend& backend, model::AuditData& auditData);

protected:
    HsmPool& mHsmPool;
    KeyDerivation& mDerivation;
    DataBaseCodec mCodec;
};

#endif// ERP_PROCESSING_CONTEXT_DATABASE_COMMONDATABASEFRONTEND_HXX
