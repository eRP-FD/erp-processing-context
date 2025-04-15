/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
            return Outcome::DeadLetter;
        case HttpStatus::Forbidden:
        case HttpStatus::InternalServerError:
            return Outcome::Retry;
        case HttpStatus::Locked:
            return Outcome::ConsentRevoked;
        case HttpStatus::Conflict:
            return Outcome::Conflict;
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
