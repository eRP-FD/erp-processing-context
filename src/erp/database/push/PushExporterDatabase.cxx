/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/database/push/PushExporterDatabase.hxx"
#include "erp/database/push/PushExporterDatabaseException.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/compression/ZStd.hxx"
#include "shared/database/DatabaseConnectionInfo.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/JsonLog.hxx"

PushExporterDatabase::PushExporterDatabase(std::unique_ptr<PushExporterBackend>&& backend, HsmPool& hsmPool,
                                           KeyDerivation& keyDerivation)
    : mBackend(std::move(backend))
    , mHsmPool(hsmPool)
    , mKeyDerivation(keyDerivation)
{
}

PushExporterDatabase::~PushExporterDatabase() = default;

void PushExporterDatabase::healthCheck() const
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> PushExporterDatabase::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

void PushExporterDatabase::closeConnection()
{
    mBackend->closeConnection();
}

void PushExporterDatabase::commitTransaction()
{
    mBackend->commitTransaction();
}

void PushExporterDatabase::createPushEvent(const PushEventDataCollector& data)
{
    if (! data.isReadyForPushEvent())
    {
        TLOG(WARNING) << "Skipping push event creation due to incomplete data.";
        return;
    }
    try
    {
        push::erp::exporter::error::withDatabaseErrorHandling("", [this, &data]() {
            mBackend->createPushEvent({data.notificationIdentifier().value(), data.channelId().value(),
                                       data.hashedKvnr().value(), data.prescriptions()});
        });
        commitTransaction();

        // Safe to access the data fields since it is already checked in `isReadyForPushEvent`.
        JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false)
            .keyValue("x_request_id", data.requestId().value_or("NOT SET"))
            .keyValue("event", "Create Push Event")
            .keyValue("channel_id", model::to_string(data.channelId().value()))
            .keyValue("prescription_id", data.prescriptionId()->toString())
            .keyValue("kvnr", data.hashedKvnr()->toHex());
    }
    catch (const PushErpExporterDatabaseException& exc)
    {
        TLOG(ERROR) << exc.what();
        JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false)
            .keyValue("x_request_id", data.requestId().value_or("NOT SET"))
            .keyValue("event", "Could not create Push Event")
            .keyValue("channel_id", model::to_string(data.channelId().value()))
            .keyValue("prescription_id", data.prescriptionId()->toString())
            .keyValue("kvnr", data.hashedKvnr()->toHex())
            .keyValue("error", exc.what());
    }
}
