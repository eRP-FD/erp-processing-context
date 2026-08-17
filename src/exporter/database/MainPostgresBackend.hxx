/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX

#include "shared/database/CommonPostgresBackend.hxx"

namespace db_model
{
struct AppRegistration;
}
namespace exporter
{

class MainPostgresBackend : public CommonPostgresBackend
{
public:
    MainPostgresBackend();

    void healthCheck() override;

    PostgresConnection& connection() const override;

    std::vector<db_model::AppRegistration> retrieveAppRegistrations(const db_model::HashedKvnr& kvnrHashed,
                                                                    const std::string& channelId) const;
    void updateEncryptionKey(const db_model::HashedKvnr& kvnrHashed, const db_model::HashedId& pushkeyHashed,
                             const db_model::HashedId& appIdHashed, const db_model::EncryptedBlob& encryptionKey);
    void deletePushKey(const db_model::HashedKvnr& hashedKvnr, const db_model::HashedId& hashedPushKey);

private:
    static PostgresConnection& threadConnection();
};

}; // namespace exporter

#endif//ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX
