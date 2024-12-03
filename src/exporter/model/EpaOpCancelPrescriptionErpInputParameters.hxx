/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPCANCELPRESCRIPTIONERPINPUTPARAMETERS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPCANCELPRESCRIPTIONERPINPUTPARAMETERS_HXX

#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"

namespace model
{
class PrescriptionId;

// NOLINTNEXTLINE(bugprone-exception-escape)
class EPAOpCancelPrescriptionERPInputParameters : public Parameters<EPAOpCancelPrescriptionERPInputParameters>
{
public:
    EPAOpCancelPrescriptionERPInputParameters(const model::PrescriptionId& prescriptionId,
                                              const model::Timestamp& authoredOn);
    static constexpr auto profileType = ProfileType::CancelPrescriptionErpOp;
    friend class Resource;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

protected:
    using Parameters::Parameters;
};

extern template class Resource<EPAOpCancelPrescriptionERPInputParameters>;
extern template class Parameters<EPAOpCancelPrescriptionERPInputParameters>;

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPCANCELPRESCRIPTIONERPINPUTPARAMETERS_HXX
