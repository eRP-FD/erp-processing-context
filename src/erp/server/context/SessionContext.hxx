/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX

#include "erp/database/Database.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/BaseSessionContext.hxx"

#include <boost/core/noncopyable.hpp>
#include <memory>
#include <optional>
#include <string_view>


namespace model {
class KBVMultiplePrescription;
}

class PcServiceContext;


/**
 * A session context is a bag of values that are only accessible, with one exception, only from a single thread
 * and visible only to a single request handler.
 * The exception is a reference to the, effectively global, ServiceContext.
 */
class SessionContext : public BaseSessionContext, private boost::noncopyable
{
public:
    SessionContext(PcServiceContext& serviceContext, ServerRequest& request, ServerResponse& response, AccessLog& log,
                   model::Timestamp initSessionTime = model::Timestamp::now());

    PcServiceContext& serviceContext; // Specialized type (identical to BaseSessionContext::baseServiceContext)
    bool callerWantsJson;
    std::chrono::microseconds backendDuration{0};

    AuditDataCollector& auditDataCollector();
    Database* database();
    std::unique_ptr<Database> releaseDatabase();
    const model::Timestamp& sessionTime() const;

    void addOuterResponseHeaderField(std::string_view key, std::string_view value);
    const Header::keyValueMap_t& getOuterResponseHeaderFields() const;

    void fillMvoBdeV2(const std::optional<model::KBVMultiplePrescription>& mPExt);

private:
    std::unique_ptr<AuditDataCollector> mAuditDataCollector;
    std::unique_ptr<Database> mDatabase;
    model::Timestamp mSessionTime;
    Header::keyValueMap_t mOuterResponseHeaderFields;
};


#endif
