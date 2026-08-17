/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX
#define ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX

#include "MainPostgresBackend.hxx"
#include "shared/database/CommonDatabaseFrontend.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/model/AuditData.hxx"


class AuditDataCollector;
namespace model
{
class PushNotificationEvent;
class PushNotification;
class PushNotificationContext;
}
namespace exporter
{

class MainDatabaseFrontendInterface
{
public:
    virtual ~MainDatabaseFrontendInterface() = default;
    virtual std::shared_ptr<Compression> compressionInstance() = 0;
    virtual std::tuple<SafeString, BlobId> auditEventKey(const db_model::HashedKvnr& hashedKvnr) = 0;
    virtual std::string storeAuditEventData(const AuditDataCollector& auditDataCollector) = 0;
    virtual void healthCheck() = 0;
    virtual std::optional<DatabaseConnectionInfo> getConnectionInfo() const = 0;
    virtual void commitTransaction() = 0;
    virtual std::vector<model::PushNotificationContext>
    retrievePushNotificationEventData(const model::PushNotificationEvent& pushNotificationEvent) = 0;
    virtual void updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                                     const model::PushNotificationContext& pushNotificationEvent) = 0;
    virtual void deletePushKey(const model::HashedKvnr& hashedKvnr, const model::PushKey& pushKey) const = 0;
};

class MainDatabaseFrontend : public MainDatabaseFrontendInterface
{
public:
    using Factory = std::function<std::unique_ptr<MainDatabaseFrontendInterface>(HsmPool&, KeyDerivation&)>;

    MainDatabaseFrontend(std::unique_ptr<exporter::MainPostgresBackend>&& backend, HsmPool& hsmPool,
                         KeyDerivation& keyDerivation);
    ~MainDatabaseFrontend() override;
    std::shared_ptr<Compression> compressionInstance() override;
    std::tuple<SafeString, BlobId> auditEventKey(const db_model::HashedKvnr& hashedKvnr) override;
    std::string storeAuditEventData(const AuditDataCollector& auditDataCollector) override;
    void healthCheck() override;
    std::optional<DatabaseConnectionInfo> getConnectionInfo() const override;
    void commitTransaction() override;

    std::vector<model::PushNotificationContext>
    retrievePushNotificationEventData(const model::PushNotificationEvent& pushNotificationEvent) override;
    void updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed,
                             const model::PushNotificationContext& pushNotificationEvent) override;
    void deletePushKey(const model::HashedKvnr& hashedKvnr, const model::PushKey& pushKey) const override;

private:
    std::unique_ptr<exporter::MainPostgresBackend> mBackend;
    std::unique_ptr<CommonDatabaseFrontend> mCommonDatabaseFrontend;
    KeyDerivation& mDerivation;
    DataBaseCodec mCodec;
};

};// namespace exporter;

#endif//ERP_PROCESSING_CONTEXT_MAINDATABASEFRONTEND_HXX
