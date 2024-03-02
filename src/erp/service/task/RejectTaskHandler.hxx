/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_REJECTTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_REJECTTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class RejectTaskHandler : public TaskHandlerBase
{
public:
    RejectTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest (PcSessionContext& session) override;
};


#endif
