/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class CloseTaskHandler
    : public TaskHandlerBase
{
public:
    CloseTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest (PcSessionContext& session) override;

private:
    static std::vector<model::MedicationDispense> medicationDispensesFromBody(PcSessionContext& session);
};


#endif
