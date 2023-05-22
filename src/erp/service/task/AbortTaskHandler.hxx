/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_ABORTTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_ABORTTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class AbortTaskHandler : public TaskHandlerBase
{
public:
    AbortTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest (PcSessionContext& session) override;

private:
    void checkAccessValidityOutsidePharmacy(AuditDataCollector& auditDataCollector,
                                            const std::string& professionOIDClaim, const model::Task& task,
                                            const ServerRequest& request);
    void checkAccessValidity(AuditDataCollector& auditDataCollector, const std::string& professionOIDClaim,
                             const model::Task& task, const ServerRequest& request);
    void checkAccessValidityPharmacy(const model::Task& task, const ServerRequest& request);
};


#endif
