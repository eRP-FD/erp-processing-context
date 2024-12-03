/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_CLIENT_EPAMEDIACTIONCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_CLIENT_EPAMEDIACTIONCLIENT_HXX

#include "exporter/model/EpaOperationOutcome.hxx"

#include <any>
#include <string>
#include <unordered_map>

namespace model
{
class Kvnr;
class PrescriptionId;
}

class IEpaMedicationClient
{
public:
    struct Response
    {
        HttpStatus httpStatus;
        model::NumberAsStringParserDocument body;
    };

    virtual ~IEpaMedicationClient() = default;
    virtual Response sendProvidePrescription(const model::Kvnr& kvnr, const std::string& payload) = 0;
    virtual Response sendProvideDispensation(const model::Kvnr& kvnr, const std::string& payload) = 0;
    virtual Response sendCancelPrescription(const model::Kvnr& kvnr, const std::string& payload) = 0;
    virtual void addLogData(const std::string& key, const std::any& data) = 0;
};

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_CLIENT_EPAMEDIACTIONCLIENT_HXX
