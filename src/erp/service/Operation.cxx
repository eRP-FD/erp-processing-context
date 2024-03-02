/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/Operation.hxx"
#include "erp/util/Expect.hxx"

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
    constexpr std::string_view operationName_POST_Subscription         = "POST /Subscription";
    constexpr std::string_view operationName_GET_metadata              = "GET /metadata";
    // PKV specific:  ->
    constexpr std::string_view operationName_DELETE_ChargeItem_id      = "DELETE /ChargeItem/<id>";
    constexpr std::string_view operationName_GET_ChargeItem            = "GET /ChargeItem";
    constexpr std::string_view operationName_GET_ChargeItem_id         = "GET /ChargeItem/<id>";
    constexpr std::string_view operationName_POST_ChargeItem           = "POST /ChargeItem";
    constexpr std::string_view operationName_PUT_ChargeItem_id         = "PUT /ChargeItem/<id>";
    constexpr std::string_view operationName_DELETE_Consent            = "DELETE /Consent";
    constexpr std::string_view operationName_PATCH_ChargeItem_id       = "PATCH /ChargeItem/<id>";
    constexpr std::string_view operationName_GET_Consent               = "GET /Consent";
    constexpr std::string_view operationName_POST_Consent              = "POST /Consent";
    // <-
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
        case Operation::POST_Subscription:         return operationName_POST_Subscription;
        case Operation::GET_metadata:              return operationName_GET_metadata;
        // PKV specific:  ->
        case Operation::DELETE_ChargeItem_id:      return operationName_DELETE_ChargeItem_id;
        case Operation::GET_ChargeItem:            return operationName_GET_ChargeItem;
        case Operation::GET_ChargeItem_id:         return operationName_GET_ChargeItem_id;
        case Operation::POST_ChargeItem:           return operationName_POST_ChargeItem;
        case Operation::PUT_ChargeItem_id:         return operationName_PUT_ChargeItem_id;
        case Operation::DELETE_Consent:            return operationName_DELETE_Consent;
        case Operation::PATCH_ChargeItem_id:       return operationName_PATCH_ChargeItem_id;
        case Operation::GET_Consent:               return operationName_GET_Consent;
        case Operation::POST_Consent:              return operationName_POST_Consent;
        // <-

        case Operation::UNKNOWN:                   return operationName_UNKNOWN;
        case Operation::GET_notifications_opt_in:
        case Operation::GET_notifications_opt_out:
        case Operation::POST_VAU_up:
        case Operation::GET_Health:
        case Operation::POST_Admin_restart:
        case Operation::GET_Admin_configuration:
        case Operation::PUT_Admin_pn3_configuration:
            Fail2("Operation value is not supported", std::logic_error);
    }
    Fail2("Uninitialized operation enum", std::logic_error);
}
