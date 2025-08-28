/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_TASK_MARKTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_MARKTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"

namespace eu {

class PatchTaskHandler : public TaskHandlerBase
{
public:
    PatchTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;
};

} // eu

#endif//ERP_PROCESSING_CONTEXT_TASK_MARKTASKHANDLER_HXX
