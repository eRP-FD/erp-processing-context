#ifndef ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_ACCEPTTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class AcceptTaskHandler : public TaskHandlerBase
{
public:
    AcceptTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;
};


#endif
