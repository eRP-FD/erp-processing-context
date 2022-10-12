/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_TASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_TASKHANDLER_HXX


#include "erp/crypto/CadesBesSignature.hxx"
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
    /**
     * Allows to unpack CAdES-BES signature.
     * cadesBesSignatureFile - Path to a pkcs7File
     */
    static CadesBesSignature unpackCadesBesSignature(
        const std::string& cadesBesSignatureFile, TslManager& tslManager);
    static CadesBesSignature unpackCadesBesSignatureNoVerify(
        const std::string& cadesBesSignatureFile);

private:
    static CadesBesSignature doUnpackCadesBesSignature(const std::string& cadesBesSignatureFile,
                                                       TslManager* tslManager);
};


class GetAllTasksHandler
    : public TaskHandlerBase
{
public:
    GetAllTasksHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest (PcSessionContext& session) override;

private:
    model::Bundle handleRequestFromInsurant(PcSessionContext& session);

    model::Bundle handleRequestFromPharmacist(PcSessionContext& session);
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
