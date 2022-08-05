/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
    static void checkTaskPreconditions(const PcSessionContext& session, const model::Task& task);
    static void checkMultiplePrescription(const model::KbvBundle& prescription);
};


#endif
