/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "eu/GemErpEuPrMedication.hxx"
#include "shared/model/GemErpEuPrOrganization.hxx"
#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/MedicationDispense.hxx"

namespace model
{

class MedicationDispenseBundle;

class EuMedicationDispenseInfos
{
public:
    static std::optional<EuMedicationDispenseInfos> create(const MedicationDispenseBundle& bundle);

    [[nodiscard]] const GemErpEuPrPractitioner& practitioner() const;
    [[nodiscard]] const GemErpEuPrPractitionerRole& practitionerRole() const;
    [[nodiscard]] const GemErpEuPrOrganization& organization() const;

    EuMedicationDispenseInfos(EuMedicationDispenseInfos&&) = default;

private:
    EuMedicationDispenseInfos(GemErpEuPrPractitioner&& practitioner, GemErpEuPrPractitionerRole&& practitionerRole,
                              GemErpEuPrOrganization&& organization);

    GemErpEuPrPractitioner mPractitioner;
    GemErpEuPrPractitionerRole mPractitionerRole;
    GemErpEuPrOrganization mOrganization;
};

}
