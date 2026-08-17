/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/database/push/PushExporterBackend.hxx"
#include "shared/database/DatabaseCodec.hxx"

#include <functional>
#include <memory>

class HsmPool;
class KeyDerivation;
class PushEventDataCollector;

class PushExporterDatabase
{
public:
    using Factory = std::function<std::unique_ptr<PushExporterDatabase>(HsmPool&, KeyDerivation&)>;

    PushExporterDatabase(std::unique_ptr<PushExporterBackend>&& backend, HsmPool& hsmPool,
                         KeyDerivation& keyDerivation);

    ~PushExporterDatabase();

    void healthCheck() const;
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const;

    void closeConnection();

    void commitTransaction();

    void createPushEvent(const PushEventDataCollector& data);

    PushExporterDatabase(const PushExporterDatabase&) = delete;
    PushExporterDatabase(const PushExporterDatabase&&) = delete;
    PushExporterDatabase& operator= (const PushExporterDatabase&) = delete;
    PushExporterDatabase& operator= (const PushExporterDatabase&&) = delete;
private:
    std::unique_ptr<PushExporterBackend> mBackend;
    HsmPool& mHsmPool;
    KeyDerivation& mKeyDerivation;
};
