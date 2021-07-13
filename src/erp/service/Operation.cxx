#include "erp/service/Operation.hxx"

#include <stdexcept>


namespace {
    constexpr std::string_view operationName_GET_Task                  = "GET /Task";
    constexpr std::string_view operationName_GET_Task_id               = "GET /Task/<id>";
    constexpr std::string_view operationName_POST_Task_create          = "POST /Task/$create";
    constexpr std::string_view operationName_POST_Task_id_activate     = "POST /Task/<id>/$activate";
    constexpr std::string_view operationName_POST_Task_id_accept       = "POST /Task/<id>/$accept";
    constexpr std::string_view operationName_POST_Task_id_reject       = "POST /Task/<id>/$reject";
    constexpr std::string_view operationName_POST_Task_id_close        = "POST /Task/<id>/$close";
    constexpr std::string_view operationName_POST_Task_id_abort        = "POST /Task/<id>/$abort";
    constexpr std::string_view operationName_GET_MedicationDispense    = "GET /MedicationDispense";
    constexpr std::string_view operationName_GET_MedicationDispense_id = "GET /MedicationDispense/<id>";
    constexpr std::string_view operationName_GET_Communication         = "GET /Communication";
    constexpr std::string_view operationName_GET_Communication_id      = "GET /Communication/<id>";
    constexpr std::string_view operationName_POST_Communication        = "POST /Communication";
    constexpr std::string_view operationName_DELETE_Communication_id   = "DELETE /Communication/<id>";
    constexpr std::string_view operationName_GET_AuditEvent            = "GET /AuditEvent";
    constexpr std::string_view operationName_GET_AuditEvent_id         = "GET /AuditEvent/<id>";
    constexpr std::string_view operationName_GET_Device                = "GET /Device";
    constexpr std::string_view operationName_GET_metadata              = "GET /metadata";
    constexpr std::string_view operationName_UNKNOWN                   = "UNKNOWN";
}


const std::string_view& toString (Operation operation)
{
    switch(operation)
    {
        case Operation::GET_Task:                  return operationName_GET_Task;
        case Operation::GET_Task_id:               return operationName_GET_Task_id;
        case Operation::POST_Task_create:          return operationName_POST_Task_create;
        case Operation::POST_Task_id_activate:     return operationName_POST_Task_id_activate;
        case Operation::POST_Task_id_accept:       return operationName_POST_Task_id_accept;
        case Operation::POST_Task_id_reject:       return operationName_POST_Task_id_reject;
        case Operation::POST_Task_id_close:        return operationName_POST_Task_id_close;
        case Operation::POST_Task_id_abort:        return operationName_POST_Task_id_abort;
        case Operation::GET_MedicationDispense:    return operationName_GET_MedicationDispense;
        case Operation::GET_MedicationDispense_id: return operationName_GET_MedicationDispense_id;
        case Operation::GET_Communication:         return operationName_GET_Communication;
        case Operation::GET_Communication_id:      return operationName_GET_Communication_id;
        case Operation::POST_Communication:        return operationName_POST_Communication;
        case Operation::DELETE_Communication_id:   return operationName_DELETE_Communication_id;
        case Operation::GET_AuditEvent:            return operationName_GET_AuditEvent;
        case Operation::GET_AuditEvent_id:         return operationName_GET_AuditEvent_id;
        case Operation::GET_Device:                return operationName_GET_Device;
        case Operation::GET_metadata:              return operationName_GET_metadata;
        case Operation::UNKNOWN:                   return operationName_UNKNOWN;
        case Operation::GET_notifications_opt_in:
        case Operation::GET_notifications_opt_out:
        case Operation::POST_VAU_up:
        case Operation::GET_Health:
            throw std::logic_error("Operation value is not supported");
    }
    throw std::logic_error("Uninitialized operation enum");
}
