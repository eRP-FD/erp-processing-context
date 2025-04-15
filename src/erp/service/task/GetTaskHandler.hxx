/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_GETTASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_GETTASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"
#include "erp/util/search/SearchParameter.hxx"


class GetAllTasksHandler : public TaskHandlerBase
{
public:
    GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest(PcSessionContext& session) override;

private:
    void handleRequestFromInsurant(PcSessionContext& session);

    void handleRequestFromPharmacist(PcSessionContext& session);
    model::Bundle handleRequestFromPharmacist(PcSessionContext& session, const model::Kvnr& kvnr);

    std::optional<UrlArguments> urlArgumentsForTasks(
        const std::vector<SearchParameter>& searchParamsAddon = {});
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
