/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAMEDICATIONCLIENTMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAMEDICATIONCLIENTMOCK_HXX

#include "exporter/client/EpaMedicationClient.hxx"

class EpaMedicationClientMock : public IEpaMedicationClient
{
public:
    void setHttpStatusResponse(HttpStatus response);
    void setOperationOutcomeResponse(const std::string& response);
    void setExpectedInput(const std::string& input);

    Response send(const model::Kvnr& kvnr, const std::string& payload);
    Response sendProvidePrescription(const std::string& xRequestId, const model::Kvnr& kvnr,
                                     const std::string& payload) override;
    Response sendProvideDispensation(const std::string& xRequestId, const model::Kvnr& kvnr,
                                     const std::string& payload) override;
    Response sendCancelPrescription(const std::string& xRequestId, const model::Kvnr& kvnr,
                                    const std::string& payload) override;
    void addLogData(const std::string& key, const std::any& data) override
    {
        (void) key;
        (void) data;
        // Not required in mock.
    }

private:
    HttpStatus mHttpStatus{HttpStatus::OK};
    std::string mOperationOutcome;
    std::optional<std::string> mExpectedInput;
};


#endif//ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAMEDICATIONCLIENTMOCK_HXX
