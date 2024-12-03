/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX

#include "erp/database/ErpDatabaseModel.hxx"
#include "shared/database/CommonPostgresBackend.hxx"

namespace exporter
{

class MainPostgresBackend : public CommonPostgresBackend
{
public:
    MainPostgresBackend();

    void healthCheck() override;

    PostgresConnection& connection() const override;

private:
    thread_local static PostgresConnection mConnection;
};

}; // namespace exporter

#endif//ERP_PROCESSING_CONTEXT_MAINPOSTGRESBACKEND_HXX
