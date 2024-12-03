/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPPROVIDEDISPENSATIONERPINPUTPARAMETERS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPPROVIDEDISPENSATIONERPINPUTPARAMETERS_HXX

#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"

namespace model
{
class PrescriptionId;

class EPAOpProvideDispensationERPInputParameters : public Parameters<EPAOpProvideDispensationERPInputParameters>
{
public:
    EPAOpProvideDispensationERPInputParameters();
    static constexpr auto profileType = ProfileType::ProvideDispensationErpOp;
    using Parameters::Parameters;
    friend class Resource;

    void setPrescriptionId(const PrescriptionId& prescriptionId);
    void setAuthoredOn(const Timestamp& authoredOn);
    void addMedicationAndDispense(const rapidjson::Value& medicationDispense, const rapidjson::Value& medication);
    void addMedicationDispense(const rapidjson::Value& medicationDispense);
    void addMedication(const rapidjson::Value& medication);
    void setOrganization(const rapidjson::Value& organization);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
    std::vector<const rapidjson::Value*> getMedicationDispenses() const;
    std::vector<const rapidjson::Value*> getMedications() const;
    const rapidjson::Value* getOrganization() const;
};

extern template class Resource<EPAOpProvideDispensationERPInputParameters>;
extern template class Parameters<EPAOpProvideDispensationERPInputParameters>;

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPPROVIDEDISPENSATIONERPINPUTPARAMETERS_HXX
