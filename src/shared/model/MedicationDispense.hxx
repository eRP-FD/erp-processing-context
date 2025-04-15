/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_MEDICATIONDISPENSE_HXX

#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/ProfileType.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

class MedicationDispenseId;

// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationDispense : public Resource<MedicationDispense>
{
public:
    using Resource::Resource;
    [[nodiscard]] ProfileType profileType() const;

    [[nodiscard]] PrescriptionId prescriptionId() const;
    [[nodiscard]] Kvnr kvnr() const;
    [[nodiscard]] TelematikId telematikId() const;
    [[nodiscard]] Timestamp whenHandedOver() const;
    [[nodiscard]] std::optional<model::Timestamp> whenPrepared() const;
    [[nodiscard]] MedicationDispenseId id() const;
    [[nodiscard]] std::string_view medicationReference() const;
    [[nodiscard]] std::optional<Timestamp> getValidationReferenceTimestamp() const override;

    void setId(const MedicationDispenseId& id);
    void setPrescriptionId(const model::PrescriptionId& prescriptionId);
    void setKvnr(const model::Kvnr& kvnr);
    void setTelematicId(const model::TelematikId& telematikId);
    void setWhenHandedOver(const model::Timestamp& whenHandedOver);
    void setWhenPrepared(const model::Timestamp& whenPrepared);
    void setMedicationReference(std::string_view newReference);

    static constexpr auto resourceTypeName = "MedicationDispense";

private:
    friend Resource<MedicationDispense>;
    explicit MedicationDispense(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<MedicationDispense>;
}


#endif
