/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXDISPENSATIONERPOUTPUTPARAMETERS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXDISPENSATIONERPOUTPUTPARAMETERS_HXX

#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"
#include "exporter/model/EpaOperationOutcome.hxx"


namespace model
{

class EPAOpRxDispensationERPOutputParameters : public Parameters<EPAOpRxDispensationERPOutputParameters>
{
public:
    static constexpr auto profileType = ProfileType::EPAOpRxDispensationERPOutputParameters;
    using Parameters::Parameters;
    friend class Resource;
    EPAOperationOutcome::Issue getOperationOutcomeIssue() const;
};

extern template class Resource<EPAOpRxDispensationERPOutputParameters>;
extern template class Parameters<EPAOpRxDispensationERPOutputParameters>;
}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXDISPENSATIONERPOUTPUTPARAMETERS_HXX
