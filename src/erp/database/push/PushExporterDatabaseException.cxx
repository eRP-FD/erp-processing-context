/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushExporterDatabaseException.hxx"

#include <pqxx/transaction_base>

PushErpExporterDatabaseException::PushErpExporterDatabaseException(const std::string& errorMessage)
    : std::runtime_error(errorMessage)
{
}

namespace push::erp::exporter::error
{
    [[noreturn]] void translateCurrentDatabaseException(std::string_view context)
    {
        try
        {
            // throw again current exception
            throw;
        }
        catch (const pqxx::in_doubt_error&)
        {
            // Exception that might be thrown in rare cases where the connection to the database is lost while finishing a
            // database transaction, and there's no way of telling whether it was actually executed by the backend. In this
            // case the database is left in an indeterminate (but consistent) state, and only manual inspection will tell
            // which is the case.
            commonExceptionHandler(std::string{
                "Connection to database lost during committing a transaction. "
                "The transaction may or may not have been successful"});
        }
        catch (const pqxx::internal_error& exc) { commonExceptionHandler(exc); }
        catch (const pqxx::usage_error&    exc) { commonExceptionHandler(exc); }
        catch (const pqxx::failure&        exc) { commonExceptionHandler(exc); }
        catch (...)
        {
            commonExceptionHandler(std::string{"Error during "}.append(context));
        }
    }
}