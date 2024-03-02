/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class AcceptTaskHandler : public TaskHandlerBase
{
public:
    AcceptTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;

private:
    static void checkTaskPreconditions(const PcSessionContext& session, const model::Task& task,
                                       const std::string& telematikId);
    static void checkMultiplePrescription(PcSessionContext& session, const model::KbvBundle& prescription);
};


#endif
