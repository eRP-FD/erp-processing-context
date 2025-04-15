/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_TRACEID_HXX
#define EPA_LIBRARY_UTIL_TRACEID_HXX

#include <string>

namespace epa
{

/**
 * This wraps the HTTP x-trace-id header. Not to be confused with LogTrace.
 */
class TraceId
{
public:
    static constexpr std::string_view TRACEID{"x-trace-id"};

    explicit TraceId(const std::string& traceId);

    bool operator==(const TraceId& traceId) const;
    bool operator!=(const TraceId& traceId) const;

    /**
     * returns the TraceId value
     */
    const std::string& getValue() const;

    /**
     * returns the TraceId as log prefix
     */
    std::string getLogPrefix() const;

    /**
     * checks if a string is traceId conform
     */
    static bool isTraceId(const std::string& traceId);

    /**
     * cheks if the traceId is set
     */
    bool empty() const;

private:
    std::string mTraceId;
};

} // namespace epa

#endif
