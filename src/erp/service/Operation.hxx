/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_OPERATION_HXX
#define ERP_PROCESSING_CONTEXT_OPERATION_HXX

#include <string>


enum class Operation
{
    UNKNOWN = 0,

    GET_Task,
    GET_Task_id,
    POST_Task_create,
    POST_Task_id_activate,
    POST_Task_id_accept,
    POST_Task_id_reject,
    POST_Task_id_close,
    POST_Task_id_abort,
    GET_MedicationDispense,
    GET_MedicationDispense_id,
    GET_Communication,
    GET_Communication_id,
    POST_Communication,
    DELETE_Communication_id,
    GET_AuditEvent,
    GET_AuditEvent_id,
    GET_Device,
    GET_metadata,
    // PKV specific:  ->
    DELETE_ChargeItem_id,
    GET_ChargeItem,
    GET_ChargeItem_id,
    POST_ChargeItem,
    PUT_ChargeItem_id,
    DELETE_Consent_id,
    GET_Consent,
    POST_Consent,
    // <-
    GET_notifications_opt_in,
    GET_notifications_opt_out,

    POST_VAU_up,
    GET_Health,

    POST_Subscription,

    POST_Admin_restart
};

const std::string_view& toString (Operation operation);



#endif
