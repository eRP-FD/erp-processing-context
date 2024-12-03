/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EPA4ALL_PROVIDEPRESCRIPTIONERPOP_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EPA4ALL_PROVIDEPRESCRIPTIONERPOP_HXX

#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"

namespace model
{

class PrescriptionId;
class EpaMedicationRequest;

// NOLINTNEXTLINE(bugprone-exception-escape)
class EPAOpProvidePrescriptionERPInputParameters : public Parameters<EPAOpProvidePrescriptionERPInputParameters>
{
public:
    EPAOpProvidePrescriptionERPInputParameters();
    static constexpr auto profileType = ProfileType::ProvidePrescriptionErpOp;
    using Parameters::Parameters;
    friend class Resource;

    void setPrescriptionId(const PrescriptionId& prescriptionId);
    void setAuthoredOn(const Timestamp& authoredOn);
    void setMedicationRequest(const rapidjson::Value& medicationRequest);
    void setMedication(const rapidjson::Value& medication);
    void setOrganization(const rapidjson::Value& organization);
    void setPractitioner(const rapidjson::Value& practitioner);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
    PrescriptionId getPrescriptionId() const;
    Timestamp getAuthoredOn() const;
    const rapidjson::Value* getMedicationRequest() const;
    const rapidjson::Value* getMedication() const;
    const rapidjson::Value* getOrganization() const;
    const rapidjson::Value* getPractitioner() const;

private:
    void setResource(std::string_view name, const rapidjson::Value& resource);
    const rapidjson::Value* getResource(std::string_view name) const;
};

extern template class Resource<EPAOpProvidePrescriptionERPInputParameters>;
extern template class Parameters<EPAOpProvidePrescriptionERPInputParameters>;
}// model


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EPA4ALL_PROVIDEPRESCRIPTIONERPOP_HXX
