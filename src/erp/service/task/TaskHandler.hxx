/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
class KBVMultiplePrescription;
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
    static std::optional<std::string> getAccessCode(const ServerRequest& request);
    static void checkAccessCodeMatches(const ServerRequest& request, const model::Task& task);
};




#endif
