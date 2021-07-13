#ifndef ERP_PROCESSING_CONTEXT_ACCESSLOG_HXX
#define ERP_PROCESSING_CONTEXT_ACCESSLOG_HXX

#include "erp/util/JsonLog.hxx"

#include <boost/exception/exception.hpp>
#include <chrono>
#include <optional>


enum class HttpStatus;
class ServerRequest;
class ServerResponse;


/**
 * The actual logging is done by JsonLog. This class does the collecting of the information that is to be logged.
 * Constructor and destructor record start and end time. Call setInnerRequestOperation to set the endpoint name.
 * There is one AccessLog instance per SessionContext. That means the life time of a SessionContext instance
 * controls when an access log marks the start of request processing and when the access logged is written out.
 * Call update... so that an AccessLog instance can retrieve and log information from inner and outer requests and responses.
 */
class AccessLog
{
public:
    AccessLog (void);
    AccessLog (std::ostream& os);
    ~AccessLog (void);

    void setInnerRequestOperation (const std::string_view& operation);
    void updateFromOuterRequest (const ServerRequest& request);
    void updateFromInnerRequest (const ServerRequest& request);
    void updateFromInnerResponse (const ServerResponse& response);
    void updateFromOuterResponse (const ServerResponse& response);

    /**
     * Use this method to record "unusual" conditions like the inability to decrypt the inner request (which
     * prevents sending an encrypted error response).
     */
    void message (std::string_view message);

    /**
     * Add an error detail message. When called more than once then the individual message are concatenated, separated
     * by ';'.
     */
    void error (std::string_view message);

    /**
     * There are circumstances where an AccessLog object is created but output from it is undesirable.
     * The discard() method prevents any output.
     */
    void discard (void);

    /**
     * The end of request processing is usually marked by entering the AccessLog destructor. Call this method
     * to do that explicitly at an earlier time.
     */
    void markEnd (void);

    /// @see JsonLog::locationFromException(...)
    void locationFromException(const std::exception& ex);

    /// @see JsonLog::locationFromException(...)
    void locationFromException(const boost::exception& ex);

    void location(const FileNameAndLineNumber& loc);

private:
    std::optional<std::chrono::system_clock::time_point> mStartTime;
    JsonLog mLog;
    std::string mError;
};


#endif
