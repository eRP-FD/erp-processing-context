/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/TraceId.hxx"
#include "library/util/Uuid.hxx"

namespace epa
{

TraceId::TraceId(const std::string& traceId)
{
    if (traceId.empty() || ! isTraceId(traceId))
    {
        mTraceId = Uuid{}.toString();
    }
    else
    {
        mTraceId = traceId;
    }
}


const std::string& TraceId::getValue() const
{
    return mTraceId;
}


std::string TraceId::getLogPrefix() const
{
    if (empty())
    {
        return std::string{};
    }
    return "[TraceId: " + getValue() + "] ";
}


bool TraceId::operator==(const TraceId& traceId) const
{
    return mTraceId == traceId.getValue();
}


bool TraceId::operator!=(const TraceId& traceId) const
{
    return mTraceId != traceId.getValue();
}


bool TraceId::isTraceId(const std::string& traceId)
{
    return Uuid{traceId}.isValidIheUuid();
}


bool TraceId::empty() const
{
    return mTraceId.empty();
}

} // namespace epa
