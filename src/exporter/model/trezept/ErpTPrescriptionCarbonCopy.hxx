// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/model/Parameters.hxx"
#include "shared/model/Resource.hxx"


namespace model
{
class PrescriptionId;

class ErpTPrescriptionCarbonCopy : public Parameters<ErpTPrescriptionCarbonCopy>
{
public:
    ErpTPrescriptionCarbonCopy();
    static constexpr auto profileType = ProfileType::ERP_TPrescription_CarbonCopy;
    using Parameters::Parameters;
    friend class Resource;

    void setPrescriptionSignatureDate(const Timestamp& prescriptionSignatureDate);
    void setPrescriptionId(const PrescriptionId& prescriptionId);
    void setMedicationRequest(const rapidjson::Value& medicationRequest);
    void setMedication(const rapidjson::Value& medication);

    void addDispenseInformation(const rapidjson::Value& medicationDispense, const rapidjson::Value& medication);
    void setOrganization(const rapidjson::Value& organization);

    [[nodiscard]] const rapidjson::Value& rxPrescriptionMedication() const;
    // @return tuple<MedicationDispense, Medication>
    [[nodiscard]] std::tuple<gsl::not_null<const rapidjson::Value*>, gsl::not_null<const rapidjson::Value*>>
    rxDispensationInformation(size_t idx) const;
    [[nodiscard]] const rapidjson::Value& organization() const;
    [[nodiscard]] Timestamp prescriptionSignatureDate() const;
};

extern template class Resource<ErpTPrescriptionCarbonCopy>;
extern template class Parameters<ErpTPrescriptionCarbonCopy>;

}// model
