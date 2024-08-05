/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"

SessionContext::SessionContext(
    PcServiceContext& serviceContext,
    ServerRequest& request,
    ServerResponse& response,
    AccessLog& log,
    model::Timestamp initSessionTime)
    : serviceContext(serviceContext)
    , request(request)
    , response(response)
    , accessLog(log)
    , callerWantsJson(false)
    , mSessionTime(initSessionTime)
{
}


AuditDataCollector& SessionContext::auditDataCollector()
{
    if(!mAuditDataCollector)
        mAuditDataCollector = std::make_unique<AuditDataCollector>();
    return *mAuditDataCollector;
}


Database* SessionContext::database()
{
    if (! mDatabase)
        mDatabase = serviceContext.databaseFactory();
    return mDatabase.get();
}

std::unique_ptr<Database> SessionContext::releaseDatabase()
{
    return std::move(mDatabase);
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


void SessionContext::fillMvoBdeV2(const std::optional<model::KBVMultiplePrescription>& mPExt)
{
    if (mPExt && mPExt->isMultiplePrescription() && mPExt->numerator().has_value())
    {
        A_23090_02.start(
            "\"mvonr\": $mvo-nummer: Der Wert Nummer des Rezepts der Mehrfachverordnung, Datentyp Integer");
        addOuterResponseHeaderField(Header::MvoNumber, std::to_string(*mPExt->numerator()));
        A_23090_02.finish();
    }
}
