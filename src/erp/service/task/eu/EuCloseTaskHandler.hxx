/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "erp/service/MedicationDispenseHandlerBase.hxx"

namespace model
{
class GemErpEuPrPractitioner;
class GemErpEuPrPractitionerRole;
class GemErpEuPrOrganization;
};

namespace eu
{

class EuCloseTaskHandler : public MedicationDispenseHandlerBase
{
public:
    EuCloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;

private:
    static model::MedicationDispenseBundle
    createEuBundle(const model::MedicationsAndDispenses& medicationsAndDispenses,
                   const model::GemErpEuPrPractitioner& practitioner,
                   const model::GemErpEuPrPractitionerRole& practitionerRole,
                   const model::GemErpEuPrOrganization& organization);
};

}// eu
