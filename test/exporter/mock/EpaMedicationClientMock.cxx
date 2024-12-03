/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "EpaMedicationClientMock.hxx"

#include <gtest/gtest.h>

void EpaMedicationClientMock::setHttpStatusResponse(HttpStatus response)
{
    mHttpStatus = response;
}

void EpaMedicationClientMock::setOperationOutcomeResponse(const std::string& response)
{
    mOperationOutcome = response;
}

void EpaMedicationClientMock::setExpectedInput(const std::string& input)
{
    mExpectedInput = input;
}

IEpaMedicationClient::Response EpaMedicationClientMock::send(const model::Kvnr&, const std::string& payload)
{
    if (mExpectedInput)
    {
        [&] {
            EXPECT_EQ(payload, mExpectedInput);
        }();
    }
    return {.httpStatus = mHttpStatus, .body = model::NumberAsStringParserDocument::fromJson(mOperationOutcome)};
}
IEpaMedicationClient::Response EpaMedicationClientMock::sendProvidePrescription(const model::Kvnr& kvnr,
                                                                                const std::string& payload)
{
    return send(kvnr, payload);
}
IEpaMedicationClient::Response EpaMedicationClientMock::sendProvideDispensation(const model::Kvnr& kvnr,
                                                                                const std::string& payload)
{
    return send(kvnr, payload);
}
IEpaMedicationClient::Response EpaMedicationClientMock::sendCancelPrescription(const model::Kvnr& kvnr,
                                                                               const std::string& payload)
{
    return send(kvnr, payload);
}
