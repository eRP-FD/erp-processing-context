/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class AcceptTaskHandler : public TaskHandlerBase
{
public:
    AcceptTaskHandler(const OIDsByWorkflow& allowedProfessionOiDsByWorkflow);
    void handleRequest (PcSessionContext& session) override;

private:
    static void checkTaskPreconditions(const PcSessionContext& session, const model::Task& task,
                                       const std::string& telematikId);
    static void checkMultiplePrescription(PcSessionContext& session, const model::KbvBundle& prescription);
};


#endif
