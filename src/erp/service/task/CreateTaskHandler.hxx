/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CREATETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_CREATETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class CreateTaskHandler
        : public TaskHandlerBase
{
public:
    CreateTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;
};




#endif //ERP_PROCESSING_CONTEXT_CREATETASKHANDLER_HXX
