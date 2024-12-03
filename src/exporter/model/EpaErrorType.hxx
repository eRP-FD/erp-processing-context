/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAERRORTYPE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAERRORTYPE_HXX

#include "fhirtools/model/NumberAsStringParserDocument.hxx"

#include <string>

namespace model
{

class EpaErrorType
{
public:
    EpaErrorType(const NumberAsStringParserDocument& document);

    const std::string& getErrorCode() const;
    const std::string& getErrorDetail() const;

private:
    std::string mErrorCode;
    std::string mErrorDetail;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAERRORTYPE_HXX
