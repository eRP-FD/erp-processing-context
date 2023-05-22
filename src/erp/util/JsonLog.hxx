/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_JSONLOG_HXX
#define ERP_PROCESSING_CONTEXT_JSONLOG_HXX

#include "erp/util/Expect.hxx"

#include <boost/exception/exception.hpp>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string_view>


namespace {
    constexpr uint32_t LOGID_CRITICAL                             = 0x10000000;
    constexpr uint32_t LOGID_ERROR                                = 0x20000000;
    constexpr uint32_t LOGID_WARNING                              = 0x30000000;
    constexpr uint32_t LOGID_INFO                                 = 0x40000000;

    constexpr uint32_t LOGID_EXTERNAL_SERVICE_FAILURE             = 0x01000000;

    constexpr uint32_t LOGID_SERVICE_IDP                          = 0x00000001;
    constexpr uint32_t LOGID_SERVICE_TSL                          = 0x00000002;
    constexpr uint32_t LOGID_SERVICE_HSM                          = 0x00000003;
    constexpr uint32_t LOGID_SERVICE_DOS                          = 0x00000004;
    constexpr uint32_t LOGID_ACCESS                               = 0x00000005;
    constexpr uint32_t LOGID_INTERNAL                             = 0x00000006;
}


enum class LogId : uint32_t
{
    IDP_UPDATE_FAILED =  LOGID_CRITICAL | LOGID_EXTERNAL_SERVICE_FAILURE | LOGID_SERVICE_IDP,
    IDP_UPDATE_SUCCESS = LOGID_INFO     |                                  LOGID_SERVICE_IDP,

    TSL_CRITICAL =       LOGID_CRITICAL |                                  LOGID_SERVICE_TSL,
    TSL_ERROR =          LOGID_ERROR    |                                  LOGID_SERVICE_TSL,
    TSL_WARNING =        LOGID_WARNING  |                                  LOGID_SERVICE_TSL,

    HSM_CRITICAL =       LOGID_CRITICAL | LOGID_EXTERNAL_SERVICE_FAILURE | LOGID_SERVICE_HSM,
    HSM_WARNING =        LOGID_WARNING  | LOGID_EXTERNAL_SERVICE_FAILURE | LOGID_SERVICE_HSM,
    HSM_INFO =           LOGID_INFO     |                                  LOGID_SERVICE_HSM,

    DOS_ERROR =          LOGID_ERROR    | LOGID_EXTERNAL_SERVICE_FAILURE | LOGID_SERVICE_DOS,

    ACCESS =             LOGID_INFO     |                                  LOGID_ACCESS,

    INTERNAL_ERROR =     LOGID_ERROR    |                                  LOGID_INTERNAL,
    INTERNAL_WARNING =   LOGID_WARNING  |                                  LOGID_INTERNAL,

    INFO =               LOGID_INFO
};


/**
 * Write log messages in a simple JSON format. Primarily used to report error conditions that may require an operator
 * to take action.
 * Also used for success messages after an error condition has been resolved.
 * Also used for access logs.
 *
 * This class follows a builder pattern where the constructor creates a new object and its destructor will serialize the
 * JSON message and write it out. The actual logging is still done with the help of GLog.
 */
class JsonLog
{
public:
    using LogReceiver = std::function<void(std::string&&)>;

    static constexpr bool withDetailsDefault();


    JsonLog(LogId id, LogReceiver&& receiver, bool withDetails = withDetailsDefault());
    JsonLog(LogId id, std::ostream& os, bool withDetails = withDetailsDefault());
    ~JsonLog(void);

    static LogReceiver makeErrorLogReceiver();
    static LogReceiver makeWarningLogReceiver();
    static LogReceiver makeInfoLogReceiver();
    static LogReceiver makeVLogReceiver(const int32_t logLevel);

    JsonLog& message (std::string_view text);
    JsonLog& details (std::string_view details);
    JsonLog& keyValue (std::string_view key, std::string_view value);
    JsonLog& keyValue (std::string_view key, size_t value);
    JsonLog& keyValue (std::string_view key, uint16_t value);
    JsonLog& keyValue (std::string_view key, double value, uint8_t precision = 0);

    JsonLog& location(const FileNameAndLineNumber& loc);

    /// If @p ex is derived form ExceptionWrapperBase, adds location from exception; otherwise adds location "unknown"
    void locationFromException(const std::exception& ex);

    /// If @p ex is derived form ExceptionWrapperBase, adds location from exception; otherwise adds location "unknown"
    void locationFromException(const boost::exception& ex);

    /**
     * There are circumstances where an AccessLog object is created but output from it is undesirable.
     * The discard() method prevents any output from its destructor. When writeWithoutDetails() or writeWithDetails()
     * are explicitly called, they still produce output.
     */
    void discard (void);

private:
    template <typename ExceptionT>
    void locationFromException(const ExceptionT& ex);

    LogReceiver mLogReceiver;
    const bool mShowDetails;
    std::ostringstream mLog;
    std::string mDetails;

    void finish (void);
    static std::string escapeJson (const std::string& text);
};

constexpr bool JsonLog::withDetailsDefault()
{
#ifdef ENABLE_DEBUG_LOG
    return true;
#endif
    return false;
}


#endif
