/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/Outcome.hxx"

namespace eventprocessing
{


Outcome fromHttpStatus(HttpStatus httpStatus)
{
    switch (httpStatus)
    {
        case HttpStatus::OK:
            return Outcome::Success;
        case HttpStatus::BadRequest:
        case HttpStatus::Forbidden:
            return Outcome::DeadLetter;
        case HttpStatus::Conflict:
        case HttpStatus::InternalServerError:
        case HttpStatus::Locked:
            return Outcome::Retry;
        default:
            break;
    }
    TLOG(ERROR) << "unexpected HTTP status from Medication client: "
                << std::to_string(static_cast<uintmax_t>(httpStatus));
    return Outcome::DeadLetter;
}

Outcome triage(Outcome o1, Outcome o2)
{
    return std::max(o1, o2);
}

}
