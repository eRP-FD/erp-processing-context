/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_CONTEXT_SESSIONCONTEXT_HXX

#include "erp/database/Database.hxx"
#include "erp/database/push/PushEventDataCollector.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/AccessLog.hxx"
#include "shared/server/BaseSessionContext.hxx"
#include "shared/util/BdeUseCases.hxx"

#include <boost/core/noncopyable.hpp>
#include <memory>
#include <optional>
#include <string_view>


class PushErpDatabase;
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
    bool callerWantsJson{false};
    std::chrono::microseconds backendDuration{0};

    AuditDataCollector& auditDataCollector();
    PushEventDataCollector& pushEventDataCollector();
    Database* database();
    std::unique_ptr<Database> releaseDatabase();
    PushErpDatabase* pushDatabase();
    std::unique_ptr<PushErpDatabase> releasePushDatabase();
    const model::Timestamp& sessionTime() const;

    void addOuterResponseHeaderField(std::string_view key, std::string_view value);
    const Header::keyValueMap_t& getOuterResponseHeaderFields() const;

    void fillMvoBdeV2(const std::optional<model::KBVMultiplePrescription>& mPExt);

    void setBdeUseCase(bde::UseCase useCase);
    std::optional<bde::UseCase> getBdeUseCase() const;

private:
    std::unique_ptr<AuditDataCollector> mAuditDataCollector;
    std::unique_ptr<PushEventDataCollector> mPushEventDataCollector;
    std::unique_ptr<Database> mDatabase;
    std::unique_ptr<PushErpDatabase> mPushDatabase;
    model::Timestamp mSessionTime;
    Header::keyValueMap_t mOuterResponseHeaderFields;
    std::optional<bde::UseCase> mBdeUseCase;
};


#endif
