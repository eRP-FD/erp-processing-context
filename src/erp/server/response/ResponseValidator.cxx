/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/response/ResponseValidator.hxx"

#include "erp/model/Bundle.hxx"
#include "erp/util/TLog.hxx"
#include "erp/ErpRequirements.hxx"

#include "erp/common/MimeType.hxx"

#include <unordered_set>


namespace {
    std::unordered_set<Operation> allOperations =
        {
            Operation::GET_Task,
            Operation::GET_Task_id,
            Operation::POST_Task_create,
            Operation::POST_Task_id_activate,
            Operation::POST_Task_id_accept,
            Operation::POST_Task_id_reject,
            Operation::POST_Task_id_close,
            Operation::POST_Task_id_abort,
            Operation::GET_MedicationDispense,
            Operation::GET_MedicationDispense_id,
            Operation::GET_Communication,
            Operation::GET_Communication_id,
            Operation::POST_Communication,
            Operation::DELETE_Communication_id,
            Operation::GET_AuditEvent,
            Operation::GET_AuditEvent_id,
            // PKV specific:  ->
            Operation::DELETE_ChargeItem_id,
            Operation::GET_ChargeItem,
            Operation::GET_ChargeItem_id,
            Operation::POST_ChargeItem,
            Operation::PUT_ChargeItem_id,
            Operation::DELETE_Consent_id,
            Operation::GET_Consent,
            Operation::POST_Consent,
            // <-
            Operation::GET_Device,
            Operation::GET_metadata,
            Operation::POST_Subscription,
            Operation::UNKNOWN // when parsing the inner request fails the operation is unknown

            // Well, almost all operations. and POST_VAU_up are special and not included.
        };
    // A recurring set of operations.
    // Is there a better name?
    std::unordered_set<Operation> manyOperations =
        {
            Operation::GET_Task,
            Operation::GET_Task_id,
            Operation::GET_AuditEvent,
            Operation::GET_AuditEvent_id,        // Added although not present in spec
            Operation::GET_Communication,
            Operation::GET_Communication_id,     // Added although not present in spec
            Operation::GET_MedicationDispense,
            Operation::GET_MedicationDispense_id,// Added although not present in spec
            Operation::POST_Task_create,
            Operation::POST_Task_id_activate,
            Operation::POST_Task_id_accept,
            Operation::POST_Task_id_reject,
            Operation::POST_Task_id_close,
            Operation::POST_Task_id_abort,
            Operation::POST_Communication,
            Operation::DELETE_Communication_id,  // Added although not present in spec
            Operation::POST_Subscription,
            // PKV specific:  ->
            Operation::DELETE_ChargeItem_id,
            Operation::GET_ChargeItem,
            Operation::GET_ChargeItem_id,
            Operation::POST_ChargeItem,
            Operation::PUT_ChargeItem_id,
            Operation::DELETE_Consent_id,
            Operation::GET_Consent,
            Operation::POST_Consent,
            // <-
        };

    std::unordered_set<Operation> operationsWithRequestBody =
        {
            Operation::POST_Task_create,
            Operation::POST_Task_id_activate,
            Operation::POST_Task_id_close,
            Operation::POST_Communication,
            Operation::POST_Subscription,
            // PKV specific:  ->
            Operation::POST_ChargeItem,
            Operation::PUT_ChargeItem_id,
            Operation::POST_Consent,
            // <-
        };

    std::unordered_set<Operation> operationsWithResponseBody =
        {
            Operation::GET_Task,
            Operation::GET_Task_id,
            Operation::POST_Task_create,
            Operation::GET_AuditEvent,
            Operation::GET_AuditEvent_id,
            Operation::GET_Communication,
            Operation::GET_Communication_id,
            Operation::GET_MedicationDispense,
            Operation::GET_MedicationDispense_id,
            Operation::POST_Task_id_activate,
            Operation::POST_Task_id_accept,
            Operation::POST_Task_id_close,
            Operation::POST_Communication,
            Operation::POST_Subscription,
            // PKV specific:  ->
            Operation::GET_ChargeItem,
            Operation::GET_ChargeItem_id,
            Operation::POST_ChargeItem,
            Operation::PUT_ChargeItem_id,
            Operation::GET_Consent,
            Operation::POST_Consent,
            // <-
            Operation::GET_notifications_opt_in,
            Operation::GET_notifications_opt_out,
            Operation::GET_Device,
            Operation::GET_metadata
        };

    std::unordered_map<HttpStatus, std::unordered_set<Operation>> permittedOperations =
        {
            {HttpStatus::OK,                      // 200
                {
                    Operation::GET_Task,
                    Operation::GET_Task_id,
                    Operation::GET_AuditEvent,
                    Operation::GET_AuditEvent_id,
                    Operation::GET_Communication,
                    Operation::GET_Communication_id,
                    Operation::GET_MedicationDispense,
                    Operation::GET_MedicationDispense_id,
                    Operation::POST_Task_id_activate,
                    Operation::POST_Task_id_accept,
                    Operation::POST_Task_id_close,
                    // PKV specific:  ->
                    Operation::GET_ChargeItem,
                    Operation::GET_ChargeItem_id,
                    Operation::PUT_ChargeItem_id,
                    Operation::GET_Consent,
                    // <-
                    Operation::GET_notifications_opt_in,
                    Operation::GET_notifications_opt_out,
                    Operation::GET_Device,
                    Operation::GET_metadata,
                    Operation::POST_Subscription

                 // gemspec_FD_eRp_V1.1.1 is a bit vague about the exact set of values. I.e. it ends in the sentence
                 // (translated) GET, etc for all other operations
                 // This was interpreted as, 200 is valid for all operations that are not explicitly mentioned for 201
                 // and 204
         }},
            {HttpStatus::Created,                 // 201
                {
                    Operation::POST_Task_create,
                    Operation::POST_Communication,
                    // PKV specific:  ->
                    Operation::POST_ChargeItem,
                    Operation::POST_Consent,
                    // <-
                }},
            {HttpStatus::NoContent,               // 204
                {
                    Operation::POST_Task_id_abort,
                    Operation::POST_Task_id_reject,
                    Operation::DELETE_Communication_id,
                    // PKV specific:  ->
                    Operation::DELETE_ChargeItem_id,
                    Operation::DELETE_Consent_id
                    // <-
                }},
            {HttpStatus::BadRequest,              // 400
                allOperations},
            {HttpStatus::Unauthorized,            // 401
                allOperations},
            {HttpStatus::Forbidden,               // 403
                manyOperations},

            {HttpStatus::NotFound,                // 404
                {
                    Operation::GET_Task_id,
                    Operation::POST_Task_id_activate,
                    Operation::POST_Task_id_accept,
                    Operation::POST_Task_id_reject,
                    Operation::POST_Task_id_close,
                    Operation::POST_Task_id_abort,
                    Operation::GET_AuditEvent_id,
                    Operation::GET_Communication_id,
                    Operation::GET_MedicationDispense_id,
                    Operation::GET_notifications_opt_out,
                    Operation::DELETE_Communication_id,
                    // PKV specific:  ->
                    Operation::GET_ChargeItem_id,
                    Operation::PUT_ChargeItem_id,
                    Operation::DELETE_Consent_id,
                    Operation::GET_Consent,     // TODO not sure;
                    Operation::POST_Consent,
                    // <-
                }},
            {HttpStatus::MethodNotAllowed,        // 405
                {
                    // PKV specific:  ->
                    Operation::POST_ChargeItem,
                    // <-
                    Operation::UNKNOWN
                }},
            {HttpStatus::NotAcceptable,           // 406
                operationsWithResponseBody},
            {HttpStatus::RequestTimeout,          // 408
                manyOperations},
            {HttpStatus::Conflict,                // 409
                {
                    Operation::POST_Task_id_accept,
                    Operation::POST_Task_id_abort,
                    // PKV specific:  ->
                    Operation::POST_Consent,
                    // <-
                }},
            {HttpStatus::Gone,                    // 410
                {
                Operation::GET_Task_id,
                Operation::POST_Task_id_activate,
                Operation::POST_Task_id_accept,
                Operation::POST_Task_id_reject,
                Operation::POST_Task_id_close,
                Operation::POST_Task_id_abort
                }},
            {HttpStatus::UnsupportedMediaType,    // 415
                operationsWithRequestBody},
            {HttpStatus::TooManyRequests,         // 429
                manyOperations},
            {HttpStatus::InternalServerError,     // 500
                allOperations}
        };
}


void ResponseValidator::validate (ServerResponse& response, const Operation operation)
{
    const auto status = response.getHeader().status();
    const auto operations = permittedOperations.find(status);
    if (operations == permittedOperations.end())
    {
        // There is no eRp operation that would permit the status code of the response.
        A_19514.start("prevent invalid status codes");
        TVLOG(1) << "status " << static_cast<size_t>(status)
                 << " " << toString(status)
                 << " is not supported, please fix the implementation of endpoint "
                 << toString(operation);

        // Convert the response to an internal server error.
        response.setStatus(HttpStatus::InternalServerError);
        response.setBody("");
        A_19514.finish();
    }
    else if (operations->second.count(operation) == 0)
    {
        // There are eRp operations that would permit the status code of the response
        // but `operation` is not one of them.
        A_19514.start("prevent invalid status codes");
        TVLOG(1) << "operation " << toString(operation)
                 << " does not permit response status "
                 << static_cast<size_t>(status)
                 << " " << toString(status)
                 << ", please fix the implementation of endpoint "
                 << toString(operation);

        // Convert the response to an internal server error.
        response.setStatus(HttpStatus::InternalServerError);
        response.setBody("");
        A_19514.finish();
    }
    else
    {
        // Validation was successful. Print a log message.
        switch(status)
        {
            case HttpStatus::OK:
            case HttpStatus::Created:
            case HttpStatus::NoContent:
                TVLOG(1) << "request was successful with status "
                         << static_cast<size_t>(status)
                         << " " << toString(status);
                break;

            default:
                TVLOG(1) << "request failed with status "
                         << static_cast<size_t>(status)
                         << " " << toString(status);
                break;
        }
    }
}
