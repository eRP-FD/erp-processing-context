/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_OUTCOME_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_OUTCOME_HXX

#include "shared/network/message/HttpStatus.hxx"
#include "shared/util/TLog.hxx"

namespace eventprocessing
{
enum class Outcome : uint8_t
{
    Success,
    SuccessAuditFail,
    Retry,
    DeadLetter,
    Conflict,
    ConsentRevoked
};

Outcome fromHttpStatus(HttpStatus httpStatus);

Outcome triage(Outcome o1, Outcome o2);

}


#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EVENTPROCESING_OUTCOME_HXX
