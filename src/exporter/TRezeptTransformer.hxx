// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "exporter/TransformerBase.hxx"
#include "exporter/model/trezept/ErpTPrescriptionCarbonCopy.hxx"

namespace fhirtools
{
struct ValueMapping;
}

namespace model
{
class TelematikId;
class PrescriptionId;
class Bundle;
class ErpTPrescriptionOrganization;
}

class TRezeptTransformer : public TransformerBase
{
public:
    static model::ErpTPrescriptionCarbonCopy
    transform(const model::Bundle& kbvBundle, const model::Bundle& medicationDispenseBundle,
              const model::Timestamp& qesSigningTime, const model::PrescriptionId& prescriptionId,
              const model::TelematikId& fallbackTelematikId, const std::string& fallbackOrganizationName,
              const model::Bundle& vzdSearchSet);

private:
    static void transformRxPrescription(model::ErpTPrescriptionCarbonCopy& outParameter, const model::Bundle& kbvBundle,
                                        const model::Timestamp& qesSigningTime,
                                        const model::PrescriptionId& prescriptionId);
    static void transformRxDispense(model::ErpTPrescriptionCarbonCopy& outParameter,
                                    const model::Bundle& medicationDispenseBundle, const model::Bundle& vzdSearchSet,
                                    const model::TelematikId& fallbackTelematikId,
                                    const std::string& fallbackOrganizationName);
};
