/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "fhirtools/model/Timestamp.hxx"

#include <rapidjson/document.h>

#include <optional>

namespace model
{

class MedicationDispenseId;

// Reduced version of Medication Dispense resource, contains only functionality currently needed;

class MedicationDispense : public Resource<MedicationDispense>
{
public:
    [[nodiscard]] model::PrescriptionId prescriptionId() const;
    [[nodiscard]] std::string_view kvnr() const;
    [[nodiscard]] std::string_view telematikId() const;
    [[nodiscard]] fhirtools::Timestamp whenHandedOver() const;
    [[nodiscard]] std::optional<fhirtools::Timestamp> whenPrepared() const;
    [[nodiscard]] MedicationDispenseId id() const;

    void setId(const MedicationDispenseId& id);
    void setPrescriptionId(const model::PrescriptionId& prescriptionId);
    void setKvnr(const std::string_view& kvnr);
    void setTelematicId(const std::string_view& telematicId);
    void setWhenHandedOver(const fhirtools::Timestamp& whenHandedOver);
    void setWhenPrepared(const fhirtools::Timestamp& whenPrepared);

    static constexpr auto resourceTypeName = "MedicationDispense";

private:
    friend Resource<MedicationDispense>;
    explicit MedicationDispense(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
