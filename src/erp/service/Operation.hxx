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
    GET_notifications_opt_in,
    GET_notifications_opt_out,

    POST_VAU_up,
    GET_Health
};

const std::string_view& toString (Operation operation);



#endif
