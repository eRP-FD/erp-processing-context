/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_HXX
#define ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_HXX

#include "exporter/database/CommitGuard_policies.hxx"
#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontendInterface.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"

#include <memory>

template<class Database, template<class Db> class ExceptionRollbackStrategy>
    requires commit_guard_policies::DbExceptionHandlingStrategy<commit_guard_policies::ExceptionAbortStrategy, Database>
class CommitGuard
{
public:
    using database_t = Database;
    using exceptionstrategy_t = ExceptionRollbackStrategy<Database>;

    explicit CommitGuard(std::unique_ptr<database_t> db)
        : mDb(std::move(db))
    {
    }

    CommitGuard()
        : mDb(std::make_unique<database_t>())
    {
    }

    ~CommitGuard()
    {
        if (std::uncaught_exceptions() > 0)
        {
            exceptionstrategy_t::onException(*mDb);
        }
        else
        {
            mDb->commitTransaction();
        }
    }

    database_t& db()
    {
        return *mDb;
    }

private:
    std::unique_ptr<database_t> mDb;
};

using MedicationExporterCommitGuard =
    CommitGuard<MedicationExporterDatabaseFrontendInterface, commit_guard_policies::ExceptionAbortStrategy>;

using MainDatabaseCommitGuard =
    CommitGuard<exporter::MainDatabaseFrontend, commit_guard_policies::ExceptionAbortStrategy>;


#endif//ERP_PROCESSING_CONTEXT_EXPORTER_COMMITGUARD_HXX
