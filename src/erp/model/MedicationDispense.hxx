/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX

#include "erp/model/Kvnr.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/TelematikId.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

class MedicationDispenseId;

// Reduced version of Medication Dispense resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationDispense : public Resource<MedicationDispense>
{
public:
    [[nodiscard]] model::PrescriptionId prescriptionId() const;
    [[nodiscard]] Kvnr kvnr() const;
    [[nodiscard]] TelematikId telematikId() const;
    [[nodiscard]] model::Timestamp whenHandedOver() const;
    [[nodiscard]] std::optional<model::Timestamp> whenPrepared() const;
    [[nodiscard]] MedicationDispenseId id() const;

    void setId(const MedicationDispenseId& id);
    void setPrescriptionId(const model::PrescriptionId& prescriptionId);
    void setKvnr(const model::Kvnr& kvnr);
    void setTelematicId(const model::TelematikId& telematikId);
    void setWhenHandedOver(const model::Timestamp& whenHandedOver);
    void setWhenPrepared(const model::Timestamp& whenPrepared);

    static constexpr auto resourceTypeName = "MedicationDispense";

private:
    friend Resource<MedicationDispense>;
    explicit MedicationDispense(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
