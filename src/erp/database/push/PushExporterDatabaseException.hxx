/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/util/Demangle.hxx"

#include <stdexcept>

class PushErpExporterDatabaseException : public std::runtime_error
{
public:
    explicit PushErpExporterDatabaseException(const std::string& errorMessage);
};

namespace push::erp::exporter::error
{
    template<typename T>
    concept HasWhat = requires(const T& t) {
        { t.what() } -> std::convertible_to<const char*>;
    };

    template<HasWhat PqxxException>
    [[noreturn]] void commonExceptionHandler(const PqxxException& exc)
    {
        const std::string typeInfo = util::demangle(typeid(exc).name());
        throw PushErpExporterDatabaseException(
            "caught pqxx exception (" + typeInfo + ") " + exc.what());
    }

    [[noreturn]] inline void commonExceptionHandler(const std::string& error)
    {
        throw PushErpExporterDatabaseException(error);
    }

    [[noreturn]] void translateCurrentDatabaseException(std::string_view context);

    template<typename Func>
    decltype(auto) withDatabaseErrorHandling(std::string_view context, Func&& func)
    {
        try
        {
            return std::forward<Func>(func)();
        }
        catch (...)
        {
            translateCurrentDatabaseException(context);
        }
    }
}

