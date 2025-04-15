/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXPRESCRIPTIONERPOUTPUTPARAMETERS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXPRESCRIPTIONERPOUTPUTPARAMETERS_HXX

#include "shared/model/Parameters.hxx"
#include "exporter/model/EpaOperationOutcome.hxx"

namespace model
{

class EPAOpRxPrescriptionERPOutputParameters : public Parameters<EPAOpRxPrescriptionERPOutputParameters>
{
public:
    static constexpr auto profileType = ProfileType::EPAOpRxPrescriptionERPOutputParameters;
    using Parameters::Parameters;
    friend class Resource;
    EPAOperationOutcome::Issue getOperationOutcomeIssue() const;
};

extern template class Resource<EPAOpRxPrescriptionERPOutputParameters>;
extern template class Parameters<EPAOpRxPrescriptionERPOutputParameters>;

}

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPRXPRESCRIPTIONERPOUTPUTPARAMETERS_HXX
