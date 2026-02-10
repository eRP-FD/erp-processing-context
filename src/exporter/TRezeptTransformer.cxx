// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH


#include "exporter/TRezeptTransformer.hxx"
#include "exporter/model/HealthcareServiceDirectory.hxx"
#include "exporter/model/LocationDirectory.hxx"
#include "exporter/model/trezept/ErpTPrescriptionCarbonCopy.hxx"
#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "model/OrganizationDirectory.hxx"
#include "model/trezept/ErpTPrescriptionOrganization.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/MedicationDispense.hxx"

namespace
{
const fhirtools::DefinitionKey TRezeptMedicationRequestProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication-request|1.1"};
const fhirtools::DefinitionKey TRezeptMedicationProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication|1.1"};
const fhirtools::DefinitionKey TRezeptMedicationDispenseProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication-dispense|1.1"};
const fhirtools::DefinitionKey TRezeptOrganizationProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-organization|1.1"};

}

model::ErpTPrescriptionCarbonCopy TRezeptTransformer::transform(const model::Bundle& kbvBundle,
                                                                const model::Bundle& medicationDispenseBundle,
                                                                const model::Timestamp& qesSigningTime,
                                                                const model::PrescriptionId& prescriptionId,
                                                                const model::Bundle& vzdSearchSet)
{
    model::ErpTPrescriptionCarbonCopy parameters;

    transformRxPrescription(parameters, kbvBundle, qesSigningTime, prescriptionId);
    transformRxDispense(parameters, medicationDispenseBundle, vzdSearchSet);
    parameters.removeEmptyObjectsAndArrays();

    return parameters;
}
void TRezeptTransformer::transformRxPrescription(model::ErpTPrescriptionCarbonCopy& outParameter,
                                                 const model::Bundle& kbvBundle, const model::Timestamp& qesSigningTime,
                                                 const model::PrescriptionId& prescriptionId)
{
    outParameter.setPrescriptionSignatureDate(qesSigningTime);
    outParameter.setPrescriptionId(prescriptionId);

    const auto& medicationRequest = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    const auto& kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::vector<FixedValue> medicationRequestFixedValues{{.ptr = rapidjson::Pointer{"/status"}, .value = "completed"},
                                                         {.ptr = rapidjson::Pointer{"/intent"}, .value = "order"}};
    std::vector<fhirtools::ValueMapping> medicationRequestValueMappings = {
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Teratogenic",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-teratogenic-extension",
         .restrictFieldName = "url"},
        {.from = "Off-Label", .to = "off-label", .restrictFieldName = "url"},
        {.from = "GebaerfaehigeFrau", .to = "childbearing-potential", .restrictFieldName = "url"},
        {.from = "EinhaltungSicherheitsmassnahmen", .to = "security-compliance", .restrictFieldName = "url"},
        {.from = "AushaendigungInformationsmaterialien",
         .to = "hand-out-information-material",
         .restrictFieldName = "url"},
        {.from = "ErklaerungSachkenntnis", .to = "declaration-of-expertise", .restrictFieldName = "url"}};

    auto transformedMedicationRequest =
        transformResource(TRezeptMedicationRequestProfile, medicationRequest, medicationRequestValueMappings, medicationRequestFixedValues);
    remove(transformedMedicationRequest, rapidjson::Pointer{"/subject"});
    transformedMedicationRequest.removeEmptyObjectsAndArrays();
    checkSetAbsentReason(transformedMedicationRequest, "/subject", "not-permitted");
    outParameter.setMedicationRequest(transformedMedicationRequest);

    std::vector<fhirtools::ValueMapping> medicationValueMappings = {
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Packaging",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-formulation-packaging-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Form",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/"
               "medication-ingredient-darreichungsform-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Amount",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-ingredient-amount-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-packaging-size-extension"}};
    auto transformedMedication =
        transformResource(TRezeptMedicationProfile, kbvMedication, medicationValueMappings, {});
    outParameter.setMedication(transformedMedication);
}

void TRezeptTransformer::transformRxDispense(model::ErpTPrescriptionCarbonCopy& outParameter,
                                             const model::Bundle& medicationDispenseBundle,
                                             const model::Bundle& vzdSearchSet)
{
    if (vzdSearchSet.getResourceCount() == 0)
    {
        Fail2("Missing or invalid organizationDirectory.", std::runtime_error);
    }

    model::ErpTPrescriptionOrganization organization;
    std::optional<model::TelematikId> telematikId = std::nullopt;
    try
    {
        auto organizationDirectory = vzdSearchSet.getUniqueResourceByType<model::OrganizationDirectory>();
        const auto& telematikIdItem = organizationDirectory.getTelematikIdIdentifier();
        organization.setTelematikId(telematikIdItem);
        telematikId.emplace(model::TelematikId{model::NumberAsStringParserDocument::getStringValueFromValue(
            rapidjson::Pointer{"/value"}.Get(telematikIdItem))});
        organization.setId(telematikId->id());
        organization.setName(organizationDirectory.getName());
    }
    catch (const std::exception& exc)
    {
        Fail2("Missing or invalid organizationDirectory.", std::runtime_error);
    }

    const auto healthcareServices = vzdSearchSet.getResourcesByType<model::HealthcareServiceDirectory>();
    if (not healthcareServices.empty())
    {
        const auto& healthcareService = healthcareServices.front();
        if (const auto* telecom = healthcareService.telecomRaw())
        {
            organization.setTelecom(*telecom);
        }
    }
    const auto locations = vzdSearchSet.getResourcesByType<model::LocationDirectory>();
    if (not locations.empty())
    {
        const auto& location = locations.front();
        if (const auto* address = location.addressRaw())
        {
            organization.setAddress(*address);
        }
    }

    std::vector<FixedValue> medicationDispenseFixedValues{
        {.ptr = rapidjson::Pointer{"/status"}, .value = "completed"},
        {.ptr = rapidjson::Pointer{"/performer/0/actor/reference"}, .value = "Organization/" + telematikId->id()}};
    for (const auto& medicationDispense : medicationDispenseBundle.getResourcesByType<model::MedicationDispense>())
    {
        const Uuid medicationReference{medicationDispense.medicationReference()};
        Expect(medicationReference.isValidIheUuid(), "MedicationDispense without valid medicationReference");
        auto transformedDispense =
            transformResource(TRezeptMedicationDispenseProfile, medicationDispense, {}, medicationDispenseFixedValues);
        remove(transformedDispense, rapidjson::Pointer{"/performer"});
        transformedDispense.setValue(rapidjson::Pointer{"/performer/0/actor/reference"},
                                     "Organization/" + telematikId->id());
        bool foundMedication = false;
        for (const auto& medication : medicationDispenseBundle.getResourcesByType<model::GemErpPrMedication>())
        {
            const Uuid medicationId{medication.getId().value_or("")};
            Expect(medicationId.isValidIheUuid(), "Medication with invalid ID");
            if (medicationId == medicationReference)
            {
                foundMedication = true;
                auto transformedMedication = transformResource(TRezeptMedicationProfile, medication, {}, {});
                outParameter.addDispenseInformation(transformedDispense, transformedMedication);
            }
        }
        Expect(foundMedication, "referenced medication not found");
    }

    outParameter.setOrganization(organization.jsonDocument());
}
