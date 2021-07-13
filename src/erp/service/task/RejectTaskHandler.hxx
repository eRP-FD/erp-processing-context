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
