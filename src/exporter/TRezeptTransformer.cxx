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
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication-request"};
const fhirtools::DefinitionKey TRezeptMedicationProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication"};
const fhirtools::DefinitionKey TRezeptMedicationDispenseProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-medication-dispense"};
const fhirtools::DefinitionKey TRezeptOrganizationProfile{
    "https://gematik.de/fhir/erp-t-prescription/StructureDefinition/erp-tprescription-organization"};

}

model::ErpTPrescriptionCarbonCopy
TRezeptTransformer::transform(const model::Bundle& kbvBundle, const model::Bundle& medicationDispenseBundle,
                              const model::Timestamp& qesSigningTime, const model::PrescriptionId& prescriptionId,
                              const model::TelematikId& fallbackTelematikId,
                              const std::string& fallbackOrganizationName, const model::Bundle& vzdSearchSet)
{
    model::ErpTPrescriptionCarbonCopy parameters;

    transformRxPrescription(parameters, kbvBundle, qesSigningTime, prescriptionId);
    transformRxDispense(parameters, medicationDispenseBundle, vzdSearchSet, fallbackTelematikId,
                        fallbackOrganizationName);
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
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/teratogenic-extension",
         .restrictFieldName = "url"},
        {.from = "Off-Label", .to = "off-label", .restrictFieldName = "url"},
        {.from = "GebaerfaehigeFrau", .to = "childbearing-potential", .restrictFieldName = "url"},
        {.from = "EinhaltungSicherheitsmassnahmen", .to = "security-compliance", .restrictFieldName = "url"},
        {.from = "AushaendigungInformationsmaterialien",
         .to = "hand-out-information-material",
         .restrictFieldName = "url"},
        {.from = "ErklaerungSachkenntnis", .to = "declaration-of-expertise", .restrictFieldName = "url"}};

    auto transformedMedicationRequest = transformResource(TRezeptMedicationRequestProfile, medicationRequest,
                                                          medicationRequestValueMappings, medicationRequestFixedValues);
    remove(transformedMedicationRequest, rapidjson::Pointer{"/subject"});
    transformedMedicationRequest.removeEmptyObjectsAndArrays();
    transformedMedicationRequest.setValue(rapidjson::Pointer{"/subject/identifier/system"},
                                          "http://fhir.de/sid/gkv/kvid-10");
    checkSetAbsentReason(transformedMedicationRequest, "/subject/identifier/_value", "not-permitted");
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
                                             const model::Bundle& vzdSearchSet,
                                             const model::TelematikId& fallbackTelematikId,
                                             const std::string& fallbackOrganizationName)
{
    model::ErpTPrescriptionOrganization organization;
    // Set profile without version
    organization.setMetaProfile0(to_string(TRezeptOrganizationProfile));
    std::optional<model::TelematikId> telematikId = std::nullopt;
    const auto vzdResourceCount = vzdSearchSet.getResourceCount();
    if (vzdResourceCount > 0)
    {
        auto organizationDirectories = vzdSearchSet.getResourcesByType<model::OrganizationDirectory>();
        if (! organizationDirectories.empty())
        {
            Expect(organizationDirectories.size() == 1, "More than one organizationDirectory found in VZD search set");
            const auto& organizationDirectory = organizationDirectories.front();
            const auto& telematikIdItem = organizationDirectory.getTelematikIdIdentifier();
            organization.setName(organizationDirectory.getName());
            organization.setTelematikId(telematikIdItem);
            telematikId.emplace(model::TelematikId{model::NumberAsStringParserDocument::getStringValueFromValue(
                rapidjson::Pointer{"/value"}.Get(telematikIdItem))});
        }
    }
    if (! telematikId.has_value())
    {
        LOG(INFO) << "No organizationDirectory found in VZD search set, using fallback values";
        telematikId.emplace(fallbackTelematikId);
        organization.setName(fallbackOrganizationName);
        organization.setTelematikId(fallbackTelematikId);
    }
    organization.setId(telematikId->id());

    const auto healthcareServices = vzdResourceCount > 0
                                        ? vzdSearchSet.getResourcesByType<model::HealthcareServiceDirectory>()
                                        : std::vector<model::HealthcareServiceDirectory>{};
    if (not healthcareServices.empty())
    {
        const auto& healthcareService = healthcareServices.front();
        if (const auto* telecom = healthcareService.telecomRaw())
        {
            organization.setTelecom(*telecom);
        }
    }
    const auto locations = vzdResourceCount > 0 ? vzdSearchSet.getResourcesByType<model::LocationDirectory>()
                                                : std::vector<model::LocationDirectory>{};
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
