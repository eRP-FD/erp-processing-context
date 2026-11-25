/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/util/ExceptionWrapper.hxx"
#include "shared/util/HeaderLog.hxx"

#include <magic_enum/magic_enum_iostream.hpp>
#include <cstddef>
#include <optional>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>

// to be intended to be used internal only, but due to templating it is needed in the header
namespace local
{

std::string sslErrors();

// Helper for arguments... logging
template<class T>
std::ostream& operator<<(std::ostream& out, const std::optional<T>& optional)
{
    if (optional.has_value())
    {
        return out << *optional;
    }
    return out;
}

using magic_enum::ostream_operators::operator<<;
inline std::ostream& operator<<(std::ostream& out, const std::exception& ex)
{
    return out << ex.what();
}

template<class... Arguments>
std::string logArgs(const Arguments&... arguments [[maybe_unused]])
{
#ifdef ENABLE_DEBUG_LOG
    if constexpr (sizeof...(Arguments) > 0)
    {
        std::ostringstream oss;
        oss << " arguments: ";
        ((oss << arguments << ", "), ...);
        return oss.str();
    }
#endif
    return "";
}

template<class Exception, class... Arguments>
[[noreturn]]
void logAndThrow(const std::string& message, const FileNameAndLineNumber& fileNameAndLineNumber,
                 Arguments&&... arguments)
{
    HeaderLog::vlog(1, [&] {
        return std::ostringstream{} << message << " at " << fileNameAndLineNumber.fileName << ':'
                                    << fileNameAndLineNumber.line << " " << logArgs(arguments...);
    });
    throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, message, std::forward<Arguments>(arguments)...);
}


template<class Exception, class... Arguments, typename MessageFuncT>
void throwIfNot(const bool expectedTrue, const char* expression, const MessageFuncT& messageFunc,
                const FileNameAndLineNumber& fileNameAndLineNumber, Arguments&&... arguments)
{
    if (! expectedTrue)
    {
        [[unlikely]];
        auto message = messageFunc();
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << message << " at " << fileNameAndLineNumber.fileName << ':'
                                        << fileNameAndLineNumber.line << "\n    " << expression
                                        << " did not evaluate to true " << logArgs(arguments...);
        });
        throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, message,
                                                  std::forward<Arguments>(arguments)...);
    }
}

template<class Exception, class... Arguments, typename MessageFuncT>
void throwIfNotWithOpenSslErrors(const bool expectedTrue, const char* expression, const MessageFuncT& messageFunc,
                                 const FileNameAndLineNumber& fileNameAndLineNumber, Arguments&&... arguments)
{
    if (! expectedTrue)
    {
        [[unlikely]];
        std::string message{messageFunc()};
        // collect OpenSSL errors
        const std::string detailedMessage{message + " " + sslErrors()};
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << detailedMessage << " at " << fileNameAndLineNumber.fileName << ':'
                                        << fileNameAndLineNumber.line << "\n    " << expression
                                        << " did not evaluate to true " << logArgs(arguments...);
        });
        throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, detailedMessage,
                                                  std::forward<Arguments>(arguments)...);
    }
}

}// namespace local

template<typename T>
decltype(auto) value(std::optional<T>& opt, std::source_location loc = std::source_location::current())
{
    if (! opt.has_value())
    {
        throw ExceptionWrapper<std::bad_optional_access>::create(FileNameAndLineNumber(loc.file_name(), loc.line()));
    }
    return *opt;
}

template<typename T>
auto value(std::optional<T>&& opt, std::source_location loc = std::source_location::current())
{
    if (! opt.has_value())
    {
        throw ExceptionWrapper<std::bad_optional_access>::create(FileNameAndLineNumber(loc.file_name(), loc.line()));
    }
    return *std::move(opt);
}

template<typename T>
decltype(auto) value(const std::optional<T>& opt, std::source_location loc = std::source_location::current())
{
    if (! opt.has_value())
    {
        throw ExceptionWrapper<std::bad_optional_access>::create(FileNameAndLineNumber(loc.file_name(), loc.line()));
    }
    return *opt;
}
