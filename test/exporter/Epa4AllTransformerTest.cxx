/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "test/exporter/Epa4AllTransformerTest.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <gtest/gtest.h>
#include <unordered_set>


namespace EpaMedicationRequest
{
// Copied over from https://gematik.github.io/api-erp/erp_epa_mapping_details/KBV_PR_ERP_Prescription%7C1.1.0_to_EPAMedicationRequest%7C1.0.2.html
// clang-format off
std::vector<std::string> propertyValuesWillBeRetained{
"MedicationRequest.authoredOn",
"MedicationRequest.dispenseRequest",
"MedicationRequest.dispenseRequest.quantity",
"MedicationRequest.dispenseRequest.quantity.code",
"MedicationRequest.dispenseRequest.quantity.system",
"MedicationRequest.dispenseRequest.quantity.value",
"MedicationRequest.dosageInstruction.patientInstruction",
"MedicationRequest.dosageInstruction.text",
"MedicationRequest.medicationReference",
"MedicationRequest.meta.id",
"MedicationRequest.meta.extension",
"MedicationRequest.meta.versionId",
"MedicationRequest.meta.lastUpdated",
"MedicationRequest.meta.source",
"MedicationRequest.meta.security",
"MedicationRequest.meta.tag",
"MedicationRequest.note",
"MedicationRequest.note.text",
"MedicationRequest.status",
"MedicationRequest.substitution.allowedBoolean",
};

std::vector<std::string> propertyValuesNotInTarget {
// Will remain empty for now, as no source information is available:
"MedicationRequest.insurance",
"MedicationRequest.basedOn",
"MedicationRequest.category",
"MedicationRequest.contained",
"MedicationRequest.courseOfTherapyType",
"MedicationRequest.detectedIssue",
"MedicationRequest.dispenseRequest.dispenseInterval",
"MedicationRequest.dispenseRequest.expectedSupplyDuration",
"MedicationRequest.dispenseRequest.initialFill",
"MedicationRequest.dispenseRequest.initialFill.duration",
"MedicationRequest.dispenseRequest.initialFill.quantity",
"MedicationRequest.dispenseRequest.numberOfRepeatsAllowed",
"MedicationRequest.dispenseRequest.performer",
"MedicationRequest.dispenseRequest.quantity.unit",
"MedicationRequest.dispenseRequest.validityPeriod",
"MedicationRequest.doNotPerform",
"MedicationRequest.dosageInstruction.additionalInstruction",
"MedicationRequest.dosageInstruction.asNeeded[x]",
"MedicationRequest.dosageInstruction.doseAndRate",
"MedicationRequest.dosageInstruction.doseAndRate.dose[x]",
"MedicationRequest.dosageInstruction.doseAndRate.rate[x]",
"MedicationRequest.dosageInstruction.doseAndRate.type",
"MedicationRequest.dosageInstruction.maxDosePerAdministration",
"MedicationRequest.dosageInstruction.maxDosePerLifetime",
"MedicationRequest.dosageInstruction.maxDosePerPeriod",
"MedicationRequest.dosageInstruction.method",
"MedicationRequest.dosageInstruction.route",
"MedicationRequest.dosageInstruction.sequence",
"MedicationRequest.dosageInstruction.site",
"MedicationRequest.dosageInstruction.timing",
"MedicationRequest.encounter",
"MedicationRequest.eventHistory",
"MedicationRequest.extension:isBvg  (https://gematik.de/fhir/epa-medication/StructureDefinition/indicator-bvg-extension)",
"MedicationRequest.extension:multiplePrescription  (https://gematik.de/fhir/epa-medication/StructureDefinition/multiple-prescription-extension)",
"MedicationRequest.groupIdentifier",
"MedicationRequest.identifier",
"MedicationRequest.identifier:RxOriginatorProcessIdentifier",
"MedicationRequest.identifier:RxPrescriptionProcessIdentifier",
"MedicationRequest.implicitRules",
"MedicationRequest.instantiatesCanonical",
"MedicationRequest.instantiatesUri",
"MedicationRequest.language",
"MedicationRequest.meta.lastUpdated",
"MedicationRequest.meta.security",
"MedicationRequest.meta.source",
"MedicationRequest.meta.tag",
"MedicationRequest.meta.versionId",
"MedicationRequest.note.author[x]",
"MedicationRequest.note.time",
"MedicationRequest.performer",
"MedicationRequest.performerType",
"MedicationRequest.priority",
"MedicationRequest.priorPrescription",
"MedicationRequest.reasonCode",
"MedicationRequest.reasonReference",
"MedicationRequest.recorder",
"MedicationRequest.reported[x]",
"MedicationRequest.statusReason",
"MedicationRequest.substitution.reason",
"MedicationRequest.supportingInformation",
"MedicationRequest.text",
// Eigenschaft und Wert(e) werden NICHT übernommen:
//A_25946.test("KBV_PR_ERP_Prescription: Keine Übernahme von \"insurance\"-Elementen");
"MedicationRequest.insurance",
// Property and value(s) will NOT be retained:
// Set by the Medication Service:
"MedicationRequest.requester.reference",
"MedicationRequest.subject.reference",
"MedicationRequest.dosageInstruction.extension.0"
};

// F_004.test("")
std::vector<ExtensionMapping> mappedExtensions
{
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription", "https://gematik.de/fhir/epa-medication/StructureDefinition/multiple-prescription-extension",
std::vector<MappedValue>{{"Extension.extension.0.url", "indicator"},{"Extension.extension.1.url", "counter", true},{"Extension.extension.2.url", "period", true},{"Extension.extension.3.url", "id", true}},
std::vector<std::string>{"Extension.extension.0.valueBoolean", "Extension.extension.1.valueRatio", "Extension.extension.2.valuePeriod", "Extension.extension.3.valueIdentifier"},
std::vector<std::string>{}}
};

std::vector<MappedValue> mappedValues{
//A_25946.test("KBV_PR_ERP_Prescription: Setzen des Pattern \"filler-order\" für .intent");
{"MedicationRequest.intent", "filler-order"},
//{"MedicationRequest.subject.reference.extension.0.url", "http://hl7.org/fhir/StructureDefinition/data-absent-reason"},
//{"MedicationRequest.subject.reference.extension.0.valueCode", "unknown"},
};

// TODO: Die Referenz der KBV-Medikation wird durch eine Referenz auf eine EPA-Medikation ersetzt:
// MedicationRequest.medication[x]:medicationReference

// TODO: Die Referenz ergibt sich aus der neu erzeugten EPA-Medikation:
// MedicationRequest.medication[x]:medicationReference.reference

// clang-format on
}

namespace EpaMedication
{
// Copied over from https://gematik.github.io/api-erp/erp_epa_mapping_details/KBV_PR_ERP_Medication_Compounding%7C1.1.0_KBV_PR_ERP_Medication_FreeText%7C1.1.0_KBV_PR_ERP_Medication_Ingredient%7C1.1.0_KBV_PR_ERP_Medication_PZN%7C1.1.0_to_EPAMedication%7C1.0.2.html
// clang-format off
std::vector<std::string> propertyValuesWillBeRetained{
"Medication.amount.denominator",
"Medication.amount.denominator.value",
"Medication.amount.numerator.code",
"Medication.amount.numerator.unit",
"Medication.batch",
"Medication.batch.expirationDate",
"Medication.batch.lotNumber",
"Medication.code.text",
"Medication.form",
// F_005.start("Medication.form.coding:kbvDarreichungsform");
"Medication.form.coding",
// F_005.finish();
"Medication.form.text",
};

std::vector<std::string> propertyValuesNotInTarget {
"Medication.amount.denominator.code", 
"Medication.amount.denominator.comparator", 
"Medication.amount.denominator.system", 
"Medication.amount.denominator.unit", 
"Medication.amount.numerator.comparator", 
"Medication.amount.numerator.value", 
"Medication.contained",
"Medication.form.coding.1",
"Medication.form.coding.2",
"Medication.identifier",
"Medication.implicitRules",
"Medication.language",
"Medication.manufacturer",
"Medication.manufacturer.display",
"Medication.manufacturer.identifier", 
"Medication.manufacturer.reference", 
"Medication.manufacturer.type", 
"Medication.meta.lastUpdated",
"Medication.meta.security", 
"Medication.meta.source", 
"Medication.meta.tag",
"Medication.meta.versionId", 
"Medication.status", 
"Medication.text",
};

// mapped in all Medications
std::vector<ExtensionMapping> mappedExtensions{
// F_005.start("Medication.extension:drugCategory");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category", "https://gematik.de/fhir/epa-medication/StructureDefinition/drug-category-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
// F_005.start("Medication.extension:isVaccine");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-id-vaccine-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
// F_005.start("Medication.extension:normSizeCode");
{"http://fhir.de/StructureDefinition/normgroesse", "http://fhir.de/StructureDefinition/normgroesse",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
};

// special mappings in namespaces
namespace KbvCompounding{
std::vector<ExtensionMapping> mappedExtensions
{
// F_005.start("Medication.amount.numerator.extension:totalQuantity");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-total-quantity-formulation-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
// F_005.start("Medication.extension:manufacturingInstructions");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_CompoundingInstruction", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-manufacturing-instructions-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Form", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-ingredient-darreichungsform-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.start("Medication.ingredient.strength.extension:amountText");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Amount", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-ingredient-amount-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
// F_005.start("Medication.extension:Verpackung");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Packaging", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-formulation-packaging-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}}
// F_005.finish();
};
}
namespace KbvIngredient {
std::vector<ExtensionMapping> mappedExtensions{
// F_005.start("Medication.amount.numerator.extension:packagingSize");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-packaging-size-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}}
// F_005.finish();
};
}

namespace KbvPzn {
std::vector<ExtensionMapping> mappedExtensions{
// F_005.start("Medication.amount.numerator.extension:packagingSize");
{"https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize", "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-packaging-size-extension",
std::vector<MappedValue>{},
std::vector<std::string>{},
std::vector<std::string>{}},
// F_005.finish();
};
std::vector<MappedValue> mappedValues{
{"Medication.code.coding.0.system", "http://fhir.de/CodeSystem/ifa/pzn"}
};
}

// clang-format on
}

namespace Organization
{
// clang-format off
std::vector<std::string> propertyValuesWillBeRetained {
"Organization.telecom",
"Organization.telecom.system",
"Organization.telecom.value",
"Organization.address.use",
"Organization.address.text",
"Organization.address.line",
"Organization.address.city",
"Organization.address.state",
"Organization.address.postalCode",
"Organization.address.country",
};
std::vector<std::string> propertyValuesNotInTarget {
"Organization.active",
"Organization.alias",
"Organization.contact",
"Organization.contact.address",
"Organization.contact.name",
"Organization.contact.purpose",
"Organization.contact.telecom",
"Organization.contained",
"Organization.endpoint",
"Organization.extension.0",
"Organization.implicitRules",
"Organization.language",
"Organization.meta.lastUpdated",
"Organization.meta.security",
"Organization.meta.source",
"Organization.meta.tag.0.display",
"Organization.meta.tag.0.userSelected",
"Organization.meta.tag.0.version",
"Organization.meta.versionId",
"Organization.partOf",
"Organization.text"
};
std::vector<MappedValue> mappedValues {
{"Organization.type.0.coding.0.system", "https://gematik.de/fhir/directory/CodeSystem/OrganizationProfessionOID"},
{"Organization.type.0.coding.0.code", std::string{profession_oid::oid_oeffentliche_apotheke}},
{"Organization.name", std::string{Epa4AllTransformerTest::organizationNameFromJwt}}
};
// clang-format on
}

// clang-format off
namespace Practitioner {
std::vector<std::string> propertyValuesWillBeRetained {
"Practitioner.name.text",
"Practitioner.name.family",
"Practitioner.name.given",
"Practitioner.name.prefix",
"Practitioner.name.suffix"
};

// clang-format on
}

void Epa4AllTransformerTest::checkMedicationRequest(const rapidjson::Value& source, const rapidjson::Value& target,
                                                    const model::Kvnr& kvnr)
{
    checkRetainedProperties(source, target, EpaMedicationRequest::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, EpaMedicationRequest::propertyValuesNotInTarget);
    checkMappedValues(target, EpaMedicationRequest::mappedValues);
    checkExtensions(source, target, EpaMedicationRequest::mappedExtensions);

    F_010.test("Das Befüllen des MedicationRequest.subject");
    std::vector<MappedValue> mappedKvnr{
        MappedValue{.property = "MedicationRequest.subject.identifier.system",
                    .targetValue = "http://fhir.de/sid/gkv/kvid-10"},
        {.property = "MedicationRequest.subject.identifier.value", .targetValue = kvnr.id()}};

    A_25946.test("KBV_PR_ERP_Prescription: Keine Übernahme von \"extension:Notdienstgebuehr\"-Elementen");
    checkExtensionRemoved(source, target, "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_EmergencyServicesFee",
                          true);
    A_25946.test("KBV_PR_ERP_Prescription: Keine Übernahme von \"extension:Zuzahlungsstatus\"-Elementen");
    checkExtensionRemoved(source, target, "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_StatusCoPayment", false);
    A_25946.test(
        "KBV_PR_ERP_Prescription: Keine Übernahme von \"MedicationRequest.extension:Unfallinformationen\"-Elementen");
    checkExtensionRemoved(source, target, "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Accident", false);

    A_25946.test(
        "KBV_PR_ERP_Prescription: Keine Übernahme von \"dosageInstruction.extension:Dosierungskennzeichen\"-Elementen");
    const rapidjson::Pointer dosageInstructionPtr{"/dosageInstruction"};
    const auto* targetDosage = dosageInstructionPtr.Get(target);
    if (targetDosage)
    {
        checkExtensionRemoved(source, *targetDosage, "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag",
                              false);
    }

    validate(
        fhirtools::DefinitionKey{
            "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-request|1.0.3"},
        &target);
}

void Epa4AllTransformerTest::checkMedicationCompounding(const rapidjson::Value& source, const rapidjson::Value& target)
{
    checkRetainedProperties(source, target, EpaMedication::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, EpaMedication::propertyValuesNotInTarget);
    auto mappedExtensions = EpaMedication::KbvCompounding::mappedExtensions;
    std::ranges::copy(EpaMedication::mappedExtensions, std::back_inserter(mappedExtensions));
    checkExtensions(source, target, mappedExtensions);
    checkMedicationIngredientArray(source, target);
    checkMedicationCodingArray(source, target);

    A_25946.test("KBV_PR_ERP_Medication_Compounding: Keine Übernahme von \"extension:Kategorie\"-Elementen");
    checkExtensionRemoved(source, target, "https://fhir.kbv.de/StructureDefinition/KBV_EX_Base_Medication_Type", true);

    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication|1.0.3"},
        &target);
}

void Epa4AllTransformerTest::checkMedicationFreeText(const rapidjson::Value& source, const rapidjson::Value& target)
{
    checkRetainedProperties(source, target, EpaMedication::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, EpaMedication::propertyValuesNotInTarget);
    auto mappedExtensions = EpaMedication::mappedExtensions;
    checkExtensions(source, target, mappedExtensions);
    checkMedicationIngredientArray(source, target);
    checkMedicationCodingArray(source, target);

    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication|1.0.3"},
        &target);
}

void Epa4AllTransformerTest::checkMedicationIngredient(const rapidjson::Value& source, const rapidjson::Value& target)
{
    checkRetainedProperties(source, target, EpaMedication::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, EpaMedication::propertyValuesNotInTarget);
    auto mappedExtensions = EpaMedication::KbvIngredient::mappedExtensions;
    std::ranges::copy(EpaMedication::mappedExtensions, std::back_inserter(mappedExtensions));
    checkExtensions(source, target, mappedExtensions);
    checkMedicationIngredientArray(source, target);
    checkMedicationCodingArray(source, target);

    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication|1.0.3"},
        &target);
}

void Epa4AllTransformerTest::checkMedicationPzn(const rapidjson::Value& source, const rapidjson::Value& target)
{
    checkRetainedProperties(source, target, EpaMedication::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, EpaMedication::propertyValuesNotInTarget);
    auto mappedExtensions = EpaMedication::KbvPzn::mappedExtensions;
    std::ranges::copy(EpaMedication::mappedExtensions, std::back_inserter(mappedExtensions));
    checkExtensions(source, target, mappedExtensions);

    A_25946.test("KBV_PR_ERP_Medication_PZN: Keine Übernahme von \"extension:Kategorie\"-Elementen");
    checkExtensionRemoved(source, target, "https://fhir.kbv.de/StructureDefinition/KBV_EX_Base_Medication_Type", true);

    checkMappedValues(target, EpaMedication::KbvPzn::mappedValues);
    checkMedicationIngredientArray(source, target);
    checkMedicationCodingArray(source, target);

    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication|1.0.3"},
        &target);
}

void Epa4AllTransformerTest::checkOrganization(const rapidjson::Value& source, const rapidjson::Value& target)
{

    checkRetainedProperties(source, target, Organization::propertyValuesWillBeRetained);
    checkPropertiesNotInTarget(target, Organization::propertyValuesNotInTarget);
    checkMappedValues(target, Organization::mappedValues);

    A_25946.test("Überschreiben/Setzen des \"identifier:TelematikID\" mit der idNummer aus dem ACCESS_TOKEN des "
                 "verwendeten Operationsaufrufes ");
    checkIdentifier(target, model::resource::naming_system::telematicID, telematikIdFromAccessToken.id());


    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/directory/StructureDefinition/OrganizationDirectory|0.11.12"},
        &target);
}

void Epa4AllTransformerTest::checkPractitioner(const rapidjson::Value& source, const rapidjson::Value& target)
{
    checkRetainedProperties(source, target, Practitioner::propertyValuesWillBeRetained);

    A_25946.test("Überschreiben/Setzen der \"identifier:Telematik-ID\" des Arztes aus dem Signaturzertifikat der QES");
    checkIdentifier(target, model::resource::naming_system::telematicID, telematikIdFromQes.id());

    validate(
        fhirtools::DefinitionKey{"https://gematik.de/fhir/directory/StructureDefinition/PractitionerDirectory|0.11.12"},
        &target);
}

void Epa4AllTransformerTest::checkRetainedProperties(const rapidjson::Value& source, const rapidjson::Value& target,
                                                     const std::vector<std::string>& properties)
{
    std::cout << serialize(source) << "\n";
    std::cout << serialize(target) << "\n";
    for (const auto& property : properties)
    {
        const auto ptr = makePointer(property);
        const auto* sourceValue = ptr.Get(source);
        const auto* targetValue = ptr.Get(target);
        if (sourceValue)
        {
            ASSERT_TRUE(targetValue) << property;
            ASSERT_EQ(sourceValue->IsString(), targetValue->IsString());
            if (sourceValue->IsString())
            {
                EXPECT_EQ(str(*sourceValue), str(*targetValue)) << property;
            }
            else
            {
                EXPECT_EQ(serialize(*sourceValue), serialize(*targetValue)) << property;
            }
        }
    }
}
void Epa4AllTransformerTest::checkPropertiesNotInTarget(const rapidjson::Value& target,
                                                        const std::vector<std::string>& properties)
{
    for (const auto& property : properties)
    {
        const auto ptr = makePointer(property);
        const auto* targetValue = ptr.Get(target);
        EXPECT_EQ(targetValue, nullptr) << property;
    }
}
void Epa4AllTransformerTest::checkMappedValues(const rapidjson::Value& target, const std::vector<MappedValue>& mappings)
{
    for (const auto& mapping : mappings)
    {
        const auto ptr = makePointer(mapping.property);
        const auto* targetValue = ptr.Get(target);
        ASSERT_TRUE(targetValue || mapping.canBeMissing) << mapping.property;
        if (targetValue)
        {
            ASSERT_TRUE(targetValue->IsString()) << mapping.property;
            EXPECT_EQ(mapping.targetValue, str(*targetValue)) << mapping.property;
        }
    }
}
void Epa4AllTransformerTest::checkExtensions(const rapidjson::Value& source, const rapidjson::Value& target,
                                             std::vector<ExtensionMapping>& mappings)
{
    static const rapidjson::Pointer extensionArrayPtr("/extension");
    const auto* extensionArray = extensionArrayPtr.Get(target);
    if (extensionArray)
    {
        for (const auto& extension : extensionArray->GetArray())
        {
            static const rapidjson::Pointer urlPointer("/url");
            const auto* url = urlPointer.Get(extension);
            bool foundMapping = false;
            for (auto& mapping : mappings)
            {
                if (mapping.targetUrl == str(*url))
                {
                    foundMapping = true;
                    const auto* sourceExtension = findExtension(source, mapping.sourceUrl);
                    ASSERT_NE(sourceExtension, nullptr)
                        << "Extension not found in source, mapping definition in test must be wrong";
                    checkRetainedProperties(*sourceExtension, extension, mapping.retainedValues);
                    checkPropertiesNotInTarget(extension, mapping.emptyInTarget);
                    checkMappedValues(extension, mapping.mappedValues);
                    mapping.found = true;
                }
            }
            EXPECT_TRUE(foundMapping) << "Found extension not allowed in target: " << str(*url);
        }
    }
    for (auto& mapping : mappings)
    {
        EXPECT_EQ(mapping.found, findExtension(source, mapping.sourceUrl) != nullptr)
            << "extension not found in target: " << mapping.targetUrl;
    }
}
void Epa4AllTransformerTest::checkExtensionRemoved(const rapidjson::Value& source, const rapidjson::Value& target,
                                                   const std::string& extensionUrl, bool forceSource)
{
    if (forceSource)
    {
        auto sourceResource = model::UnspecifiedResource::fromJson(source);
        auto sourceExtension = sourceResource.getExtension(extensionUrl);
        ASSERT_TRUE(sourceExtension.has_value());
    }
    auto targetResource = model::UnspecifiedResource::fromJson(target);
    auto targetExtension = targetResource.getExtension(extensionUrl);
    ASSERT_FALSE(targetExtension.has_value());
}
void Epa4AllTransformerTest::checkIdentifier(const rapidjson::Value& resource, const std::string_view& expectedSystem,
                                             const std::string_view& expectedValue)
{
    const auto* identifiers = rapidjson::Pointer{"/identifier"}.Get(resource);
    ASSERT_TRUE(identifiers && identifiers->IsArray());
    bool found = false;
    for (const auto& identifier : identifiers->GetArray())
    {
        const auto* system = rapidjson::Pointer{"/system"}.Get(identifier);
        ASSERT_TRUE(system && system->IsString());
        if (model::NumberAsStringParserDocument::getStringValueFromValue(system) == expectedSystem)
        {
            found = true;
            const auto* value = rapidjson::Pointer{"/value"}.Get(identifier);
            ASSERT_TRUE(value && value->IsString());
            EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(value), expectedValue);
            break;
        }
    }
    EXPECT_TRUE(found) << "Telematik-ID not found in Organization.identifiers array";
}
void Epa4AllTransformerTest::checkMedicationIngredientArray(const rapidjson::Value& source,
                                                            const rapidjson::Value& target)
{
    static const rapidjson::Pointer ingredientArrayPtr{"/ingredient"};
    const auto* sourceArray = ingredientArrayPtr.Get(source);
    const auto* targetArray = ingredientArrayPtr.Get(target);
    ASSERT_EQ(sourceArray != nullptr, targetArray != nullptr);
    if (sourceArray)
    {
        ASSERT_EQ(sourceArray->GetArray().Size(), targetArray->GetArray().Size());
        for (unsigned int i = 0; i < sourceArray->GetArray().Size(); ++i)
        {
            const auto& sourceItem = sourceArray->GetArray()[i];
            const auto& targetItem = targetArray->GetArray()[i];
            static const rapidjson::Pointer textPtr{"/itemCodeableConcept/text"};
            const auto sourceText = model::NumberAsStringParserDocument::getOptionalStringValue(sourceItem, textPtr);
            const auto targetText = model::NumberAsStringParserDocument::getOptionalStringValue(targetItem, textPtr);
            EXPECT_EQ(sourceText, targetText);
            static const rapidjson::Pointer codingArrayPtr{"/coding"};
            const auto* sourceCodingArray = codingArrayPtr.Get(source);
            const auto* targetCodingArray = codingArrayPtr.Get(target);
            if (sourceCodingArray)
            {
                for (const auto& item : sourceCodingArray->GetArray())
                {
                    static const rapidjson::Pointer systemPtr{"/system"};
                    static const rapidjson::Pointer codePtr{"/code"};
                    const auto system = model::NumberAsStringParserDocument::getOptionalStringValue(item, systemPtr);
                    ASSERT_TRUE(system);
                    if (system != "http://fhir.de/CodeSystem/ifa/pzn")
                    {
                        ASSERT_TRUE(targetCodingArray);
                        const auto* targetCodingItem =
                            model::NumberAsStringParserDocument::findMemberInArray(targetArray, systemPtr, *system);
                        ASSERT_TRUE(targetCodingItem);
                        const auto targetCode =
                            model::NumberAsStringParserDocument::getOptionalStringValue(*targetCodingItem, codePtr);
                        const auto sourceCode =
                            model::NumberAsStringParserDocument::getOptionalStringValue(item, codePtr);
                        EXPECT_EQ(targetCode, sourceCode) << *system;
                    }
                    else
                    {
                        EXPECT_FALSE(
                            model::NumberAsStringParserDocument::findMemberInArray(targetArray, systemPtr, *system));
                    }
                }
            }
        }
    }
}
void Epa4AllTransformerTest::checkMedicationCodingArray(const rapidjson::Value& source, const rapidjson::Value& target)
{
    static const std::unordered_set<std::string_view> allowList{
        model::resource::code_system::pzn, model::resource::code_system::atc, model::resource::code_system::ask,
        model::resource::code_system::sct};
    static const rapidjson::Pointer codingArrayPtr{"/code/coding"};
    static const rapidjson::Pointer systemPtr{"/system"};
    const auto* sourceArray = codingArrayPtr.Get(source);
    const auto* targetArray = codingArrayPtr.Get(target);
    for (const auto& item : sourceArray->GetArray())
    {
        const auto system = model::NumberAsStringParserDocument::getOptionalStringValue(item, systemPtr);
        const auto* targetItem =
            model::NumberAsStringParserDocument::findMemberInArray(targetArray, systemPtr, *system);
        if (allowList.contains(*system))
        {
            ASSERT_TRUE(targetItem);
            EXPECT_EQ(serialize(item), serialize(*targetItem));
        }
        else
        {
            EXPECT_FALSE(targetItem);
        }
    }
}
rapidjson::Pointer Epa4AllTransformerTest::makePointer(std::string_view property)
{
    std::string p{property};
    // Property format: ResourceName.property.subproperty
    // strip ResourceName.
    p = p.substr(p.find_first_of('.'));// keep the '.'
    std::ranges::replace(p, '.', '/');
    if (! p.starts_with('/'))
    {
        p = "/" + p;
    }
    return rapidjson::Pointer{p};
}
std::string Epa4AllTransformerTest::serialize(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    model::NumberAsStringParserWriter<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return {buffer.GetString()};
}
const rapidjson::Value* Epa4AllTransformerTest::findExtension(const rapidjson::Value& where, const std::string& url)
{
    static const rapidjson::Pointer extensionPtr("/extension");
    static const rapidjson::Pointer urlPtr("/url");
    const auto* array = extensionPtr.Get(where);
    return model::NumberAsStringParserDocument::findMemberInArray(array, urlPtr, url);
}
std::string Epa4AllTransformerTest::str(const rapidjson::Value& value)
{
    const auto strOpt = model::NumberAsStringParserDocument::getStringValueFromValue(&value);
    return std::string{strOpt};
}
void Epa4AllTransformerTest::validate(fhirtools::DefinitionKey targetProfileKey, const rapidjson::Value* resource,
                                      const model::Timestamp& timestamp)
{
    auto erpElement = createErpElement(targetProfileKey, resource, timestamp);
    auto validationResult = fhirtools::FhirPathValidator::validateWithProfiles(
        erpElement, erpElement->definitionPointer().element()->name(), {targetProfileKey}, {});
    EXPECT_LT(validationResult.highestSeverity(), fhirtools::Severity::error) << validationResult.summary();
}
void Epa4AllTransformerTest::checkExpression(const std::string& expression, fhirtools::DefinitionKey targetProfileKey,
                                             const rapidjson::Value* resource)
{
    auto erpElement = createErpElement(targetProfileKey, resource, model::Timestamp::now());
    auto repoView = getRepo(targetProfileKey, model::Timestamp::now());
    auto parsedExpression = fhirtools::FhirPathParser::parse(repoView.get(), expression);
    Expect3(parsedExpression, "could not parse " + expression, std::logic_error);
    auto resultCollection = parsedExpression->eval(fhirtools::Collection{erpElement});
    ASSERT_EQ(resultCollection.size(), 1) << expression << " did not evaluate";
    EXPECT_TRUE(resultCollection.boolean()->asBool());
}
std::shared_ptr<ErpElement> Epa4AllTransformerTest::createErpElement(fhirtools::DefinitionKey targetProfileKey,
                                                                     const rapidjson::Value* resource,
                                                                     const model::Timestamp& timestamp)
{
    auto repoView = getRepo(targetProfileKey, timestamp);
    const auto* targetProfile = repoView->findStructure(targetProfileKey);
    Expect(targetProfile, "profile not found in repo view " + to_string(targetProfileKey));
    auto erpElement =
        std::make_shared<ErpElement>(repoView, std::weak_ptr<const fhirtools::Element>{},
                                     fhirtools::ProfiledElementTypeInfo{targetProfile}, resource, nullptr);
    return erpElement;
}
std::shared_ptr<fhirtools::FhirStructureRepository>
Epa4AllTransformerTest::getRepo(fhirtools::DefinitionKey targetProfileKey, const model::Timestamp& timestamp)
{
    auto viewList = Fhir::instance().structureRepository(timestamp);
    auto repoView =
        viewList.match(std::addressof(Fhir::instance().backend()), targetProfileKey.url, *targetProfileKey.version);
    Expect(repoView, "No repository view found for " + to_string(targetProfileKey));
    return repoView;
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationRequestTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleMvoXml();
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedicationRequest = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);
    std::cout << params->serializeToJsonString() << std::endl;
    EXPECT_EQ(params->getPrescriptionId(), kbvBundle.getIdentifier());
    EXPECT_EQ(params->getAuthoredOn(), kbvMedicationRequest.authoredOn());
    const auto* medicationRequest = params->getMedicationRequest();
    ASSERT_NE(medicationRequest, nullptr);

    checkMedicationRequest(kbvMedicationRequest.jsonDocument(), *medicationRequest,
                           kbvBundle.getUniqueResourceByType<model::Patient>().kvnr());
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationMvoTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleMvoXml();
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);
    checkMedicationPzn(kbvMedication.jsonDocument(), *medication);
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationPZNTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .templatePrefix = ResourceTemplates::MedicationOptions::PZN,
            },
    });
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);
    checkMedicationPzn(kbvMedication.jsonDocument(), *medication);
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationCompoundingTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .templatePrefix = ResourceTemplates::MedicationOptions::COMPOUNDING,
            },
    });
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);
    checkMedicationCompounding(kbvMedication.jsonDocument(), *medication);
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationFreeTextTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .templatePrefix = ResourceTemplates::MedicationOptions::FREETEXT,
            },
    });
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);
    checkMedicationFreeText(kbvMedication.jsonDocument(), *medication);
}


TEST_F(Epa4AllTransformerTest, transformPrescriptionMedicationIngredientTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .templatePrefix = ResourceTemplates::MedicationOptions::INGREDIENT,
            },
    });
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    std::cout << params->serializeToJsonString() << std::endl;

    const auto* medication = params->getMedication();
    ASSERT_TRUE(medication);
    checkMedicationIngredient(kbvMedication.jsonDocument(), *medication);
    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
             &params->jsonDocument());

    checkExpression("parameter[0].part[2].resource.medicationReference.reference.resolve()",
                    fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                             "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
                    &params->jsonDocument());
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionOrganizationTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml();
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvOrganization = kbvBundle.getUniqueResourceByType<model::KbvOrganization>();

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    const auto* organization = params->getOrganization();
    ASSERT_TRUE(organization);
    std::cout << serialize(*organization) << std::endl;

    checkOrganization(kbvOrganization.jsonDocument(), *organization);
}

TEST_F(Epa4AllTransformerTest, transformPrescriptionPractitionerTest)
{
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml();
    auto kbvBundle = model::Bundle::fromXmlNoValidation(kbvBundleXml);
    auto kbvPractitioner = kbvBundle.getUniqueResourceByType<model::KbvPractitioner>();


    std::cout << kbvPractitioner.serializeToJsonString() << std::endl;

    std::optional<model::EPAOpProvidePrescriptionERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformPrescription(
                        kbvBundle, telematikIdFromQes, telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));
    ASSERT_TRUE(params);

    std::cout << params->serializeToJsonString() << std::endl;

    const auto* practitioner = params->getPractitioner();
    ASSERT_TRUE(practitioner);

    checkPractitioner(kbvPractitioner.jsonDocument(), *practitioner);
    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
             &params->jsonDocument());
    checkMappedValues(*practitioner, {MappedValue{.property = "Practitioner.name.0.text",
                                                  .targetValue = "AC Dr. med. Hans Topp-Glücklich"}});
}

TEST_F(Epa4AllTransformerTest, buildPractitionerName)
{
    const std::string nameJson =
        R"_({"use": "official", "_family": {"extension": [{"url": "http://fhir.de/StructureDefinition/humanname-namenszusatz", "valueString": "von"}, {"url": "http://hl7.org/fhir/StructureDefinition/humanname-own-name", "valueString": "Müller"}, {"url": "http://hl7.org/fhir/StructureDefinition/humanname-own-prefix", "valueString": "Dr."}]}, "family": "Müller", "given": ["Hans"], "_prefix": [{"extension": [{"url": "http://hl7.org/fhir/StructureDefinition/iso21090-EN-qualifier", "valueCode": "AC"}]}], "prefix": ["Prof."]})_";
    auto doc = model::NumberAsStringParserDocument::fromJson(nameJson);
    const auto name = Epa4AllTransformer::buildPractitionerName(doc);
    EXPECT_EQ(name, "AC Prof. Hans Dr. Müller von");
}

TEST_F(Epa4AllTransformerTest, transformMedicationDispense14)
{
    ResourceTemplates::MedicationDispenseBundleOptions options{
        .gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
        .medicationDispenses =
            {ResourceTemplates::MedicationDispenseOptions{
                 .whenHandedOver = model::Timestamp::now().toGermanDate(),
                 .gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
                 .medication =
                     ResourceTemplates::MedicationOptions{.version = ResourceTemplates::Versions::GEM_ERP_1_4}},
             ResourceTemplates::MedicationDispenseOptions{
                 .whenHandedOver = model::Timestamp::now().toGermanDate(),
                 .gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
                 .medication =
                     ResourceTemplates::MedicationOptions{.version = ResourceTemplates::Versions::GEM_ERP_1_4}}},
        .prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58")};
    auto medicationDispenseBundle1 = ResourceTemplates::internal_type::medicationDispenseBundle(options);
    auto medicationDispenseBundle =
        model::Bundle::fromXmlNoValidation(medicationDispenseBundle1.serializeToXmlString());
    std::optional<model::EPAOpProvideDispensationERPInputParameters> params;
    ASSERT_NO_THROW(params = Epa4AllTransformer::transformMedicationDispense(
                        medicationDispenseBundle, options.prescriptionId, model::Timestamp::now(),
                        telematikIdFromAccessToken, organizationNameFromJwt,
                        std::string{profession_oid::oid_oeffentliche_apotheke}));

    std::cout << medicationDispenseBundle.serializeToJsonString() << std::endl;
    std::cout << params->jsonDocument().serializeToJsonString() << std::endl;

    EXPECT_EQ(params->getMedications().size(), options.medicationDispenses.size());
    EXPECT_EQ(params->getMedicationDispenses().size(), options.medicationDispenses.size());

    validate(fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                      "epa-op-provide-dispensation-erp-input-parameters|1.0.3"},
             &params->jsonDocument(), model::Timestamp::fromGermanDate("2025-01-16"));
    checkExpression("parameter[0].part[3].resource.medicationReference.reference.resolve()",
                    fhirtools::DefinitionKey{"https://gematik.de/fhir/epa-medication/StructureDefinition/"
                                             "epa-op-provide-prescription-erp-input-parameters|1.0.3"},
                    &params->jsonDocument());
}
