/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_TASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_TASKHANDLER_HXX


#include "erp/service/ErpRequestHandler.hxx"

#include <optional>
#include <string>
#include <vector>

namespace model
{
class Task;
class KbvBundle;
class PrescriptionId;
}

class Pkcs7;

class TaskHandlerBase
    : public ErpRequestHandler
{
public:
    TaskHandlerBase (Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOIDs);


protected:
    static void addToPatientBundle(model::Bundle& bundle,
                                   const model::Task& task,
                                   const std::optional<model::KbvBundle>& patientConfirmation,
                                   const std::optional<model::Bundle>& receipt);

    static model::KbvBundle convertToPatientConfirmation(const model::Binary& healthcareProviderPrescription,
                                                      const Uuid& uuid, PcServiceContext& serviceContext);
    /// @brief extract and validate ID from URL
    static model::PrescriptionId parseId(const ServerRequest& request, AccessLog& accessLog);
    static void checkAccessCodeMatches(const ServerRequest& request, const model::Task& task);
};


class GetAllTasksHandler
    : public TaskHandlerBase
{
public:
    GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest (PcSessionContext& session) override;
};


class GetTaskHandler
    : public TaskHandlerBase
{
public:
    GetTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest (PcSessionContext& session) override;

private:
    static void checkTaskState(const std::optional<model::Task>& task);
    void handleRequestFromPatient(PcSessionContext& session, const model::PrescriptionId& prescriptionId,
                                         const JWT& accessToken);
    void handleRequestFromPharmacist(PcSessionContext& session, const model::PrescriptionId& prescriptionId);
};

#endif
