/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/server/SessionContext.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"

namespace exporter {

SessionContext::SessionContext(
    MedicationExporterServiceContext& serviceContext,
    ServerRequest& request,
    ServerResponse& response,
    AccessLog& log,
    model::Timestamp initSessionTime)
    : BaseSessionContext(serviceContext, request, response, log)
    , serviceContext(serviceContext)
    , mSessionTime(initSessionTime)
{
}


const model::Timestamp& SessionContext::sessionTime() const
{
    return mSessionTime;
}

void SessionContext::addOuterResponseHeaderField(std::string_view key, std::string_view value)
{
    mOuterResponseHeaderFields.emplace(key, value);
}

const Header::keyValueMap_t& SessionContext::getOuterResponseHeaderFields() const
{
    return mOuterResponseHeaderFields;
}

} // namespace exporter
