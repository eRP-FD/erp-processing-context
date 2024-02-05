/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_GETTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_GETTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"


class GetAllTasksHandler : public TaskHandlerBase
{
public:
    GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest(PcSessionContext& session) override;

private:
    model::Bundle handleRequestFromInsurant(PcSessionContext& session);

    model::Bundle handleRequestFromPharmacist(PcSessionContext& session);
};


class GetTaskHandler : public TaskHandlerBase
{
public:
    GetTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest(PcSessionContext& session) override;

private:
    static void checkTaskState(const std::optional<model::Task>& task, bool isPatient);
    void handleRequestFromPatient(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                  const JWT& accessToken);
    void handleRequestFromPharmacist(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                     const JWT& accessToken);

    static void checkPharmacyIsOwner(const model::Task& task, const JWT& accessToken);
};

#endif
