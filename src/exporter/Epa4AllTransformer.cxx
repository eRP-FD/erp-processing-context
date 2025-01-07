/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/Epa4AllTransformer.hxx"
#include "erp/model/KbvComposition.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/model/DataAbsentReason.hxx"
#include "exporter/model/OrganizationDirectory.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "model/EpaMedicationPznIngredient.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/model/ResourceNames.hxx"

#include <erp/model/KbvMedicationCompounding.hxx>
#include <unordered_set>

namespace
{
const fhirtools::DefinitionKey EpaMedicationRequestProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-request|1.0.3"};
const fhirtools::DefinitionKey EpaMedicationProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication|1.0.3"};
const fhirtools::DefinitionKey EpaMedicationIngredientProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-pzn-ingredient|1.0.3"};
const fhirtools::DefinitionKey EpaOrganizationProfile{
    "https://gematik.de/fhir/directory/StructureDefinition/OrganizationDirectory|0.11.12"};
const fhirtools::DefinitionKey EpaPractitionerProfile{
    "https://gematik.de/fhir/directory/StructureDefinition/PractitionerDirectory|0.11.12"};
const fhirtools::DefinitionKey BundleProfile{"http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1"};
const fhirtools::DefinitionKey EpaMedicationDispenseProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-dispense|1.0.3"};
}


model::EPAOpProvidePrescriptionERPInputParameters
Epa4AllTransformer::transformPrescription(const model::Bundle& kbvBundle, const model::TelematikId& telematikIdFromQes,
                                          const model::TelematikId& telematikIdFromJwt,
                                          const std::string_view& organizationNameFromJwt,
                                          const std::string& organizationProfessionOid)
{
    model::EPAOpProvidePrescriptionERPInputParameters parameters{};
    parameters.setId(Uuid{}.toString());
    parameters.setPrescriptionId(kbvBundle.getIdentifier());

    const auto& medicationRequest = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    const auto& kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();
    Expect(kbvMedication.getId().has_value(), "Medication resource without ID");
    F_001.start("set provide-prescription.authoredOn to MedicationRequest.authoredOn");
    parameters.setAuthoredOn(medicationRequest.authoredOn());
    F_001.finish();

    const auto& patient = kbvBundle.getUniqueResourceByType<model::Patient>();

    // medicationRequest
    Uuid medicationRequestId{};
    F_004.start("mapping of MedicationRequest.extension:Mehrfachverordnung to "
                "MedicationRequest.extension:multiplePrescription");
    std::vector<fhirtools::ValueMapping> medicationRequestValueMappings = {
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/multiple-prescription-extension"},
        {.from = "Kennzeichen", .to = "indicator", .restrictFieldName = "url"},
        {.from = "Nummerierung", .to = "counter", .restrictFieldName = "url"},
        {.from = "Zeitraum", .to = "period", .restrictFieldName = "url"},
        {.from = "ID", .to = "id", .restrictFieldName = "url"},
    };
    A_25946.start("KBV_PR_ERP_Prescription: Setzen des Pattern \"filler-order\" für .intent");
    F_010.start("Das Befüllen des MedicationRequest.subject");
    F_002.start("rewrite /medicationReference/reference");
    std::vector<FixedValue> medicationRequestFixedValues{
        {.ptr = rapidjson::Pointer{"/intent"}, .value = "filler-order"},
        {.ptr = rapidjson::Pointer{"/id"}, .value = medicationRequestId.toString()},
        {.ptr = rapidjson::Pointer{"/subject/identifier/system"},
         .value = std::string{model::resource::naming_system::gkvKvid10}},
        {.ptr = rapidjson::Pointer{"/subject/identifier/value"}, .value = patient.kvnr().id()},
        {.ptr = rapidjson::Pointer{"/medicationReference/reference"},
         .value = "Medication/" + std::string{*kbvMedication.getId()}}};
    F_002.finish();
    F_010.finish();
    A_25946.finish();
    auto transformedMedicationRequest = transformResource(EpaMedicationRequestProfile, medicationRequest,
                                                          medicationRequestValueMappings, medicationRequestFixedValues);
    A_25946.start("KBV_PR_ERP_Prescription: Keine Übernahme von \"requester.reference\"-Elementen");
    remove(transformedMedicationRequest, rapidjson::Pointer("/requester/reference"));
    A_25946.finish();
    A_25946.start("KBV_PR_ERP_Prescription: Keine Übernahme von \"insurance\"-Elementen");
    remove(transformedMedicationRequest, rapidjson::Pointer("/insurance"));
    A_25946.finish();
    A_25946.start("KBV_PR_ERP_Prescription: Keine Übernahme von \"subject.reference\"-Elementen");
    remove(transformedMedicationRequest, rapidjson::Pointer("/subject/reference"));
    A_25946.finish();
    //checkSetAbsentReason(transformedMedicationRequest, "/subject/reference");
    parameters.setMedicationRequest(transformedMedicationRequest);

    // medication
    parameters.setMedication(transformKbvMedication(kbvMedication));
    F_009.finish();
    F_008.finish();

    // organization
    std::vector<fhirtools::ValueMapping> organizationValueMappings = {};
    F_006b.start("organizationName → aus dem ACCESS_TOKEN der Anfrage");
    std::vector<FixedValue> organizationFixedValues{
        {.ptr = rapidjson::Pointer{"/name"}, .value = std::string{organizationNameFromJwt}}};

    F_007.start(
        "Beim Mapping der Organization bei providePrescription (siehe 006b) sollen die Extensions in das OpenSlicing "
        "des Organization-Profils des VZD übernommen werden, auch wenn diese nicht explizit dort modelliert sind.");
    auto transformedOrganization =
        transformResource(EpaOrganizationProfile, kbvBundle.getUniqueResourceByType<model::KbvOrganization>(),
                          organizationValueMappings, organizationFixedValues);
    F_006b.start("professionOid from ACCESS_TOKEN");
    addTypeProfessionOid(transformedOrganization, organizationProfessionOid);
    F_006b.finish();
    addMetaTagOriginLdap(transformedOrganization);

    A_25946.start("Überschreiben/Setzen des \"identifier:TelematikID\" mit der idNummer aus dem ACCESS_TOKEN des "
                  "verwendeten Operationsaufrufes");
    addTelematikIdentifier(transformedOrganization, telematikIdFromJwt.id(), true);
    A_25946.finish();

    parameters.setOrganization(transformedOrganization);

    // practitioner
    std::vector<fhirtools::ValueMapping> practitionerValueMappings = {};
    std::vector<FixedValue> practitionerFixedValues{};

    auto repoView = getRepo(BundleProfile);
    const auto* bundleProfile = repoView->findStructure(BundleProfile);
    Expect(bundleProfile, "profile not found: " + to_string(BundleProfile));
    auto erpElement = std::make_shared<ErpElement>(repoView, std::weak_ptr<const fhirtools::Element>{},
                                                   fhirtools::ProfiledElementTypeInfo{bundleProfile},
                                                   &kbvBundle.jsonDocument(), nullptr);
    F_016.start(
        "resolve the Composition author reference, as there might be 1..2 practitioners (unordered) in the bundle.");
    const std::string expression =
        "Bundle.entry.resource.ofType(Composition).author.where(type='Practitioner').resolve()";
    auto resolveReferenceExpression = fhirtools::FhirPathParser::parse(repoView.get(), expression);
    Expect3(resolveReferenceExpression, "could not parse resolveReferenceExpression", std::logic_error);
    auto resolvedReferenceCollection = resolveReferenceExpression->eval(fhirtools::Collection{erpElement});
    Expect(resolvedReferenceCollection.size() == 1, expression + " could not be resolved");
    auto resolvedRef = resolvedReferenceCollection.single();
    Expect(resolvedRef != nullptr, expression + " could not be resolved");
    F_016.finish();
    auto practitioner =
        model::UnspecifiedResource::fromJson(*std::dynamic_pointer_cast<const ErpElement>(resolvedRef)->asJson());
    const auto* humanNameElement = rapidjson::Pointer{"/name/0"}.Get(practitioner.jsonDocument());
    Expect(humanNameElement, "name field missing in practitioner");
    practitioner.setValue(rapidjson::Pointer{"/name/0/text"}, buildPractitionerName(*humanNameElement));
    auto transformedPractitioner =
        transformResource(EpaPractitionerProfile, practitioner, practitionerValueMappings, practitionerFixedValues);
    A_25946.start("Überschreiben/Setzen der \"identifier:Telematik-ID\" des Arztes aus dem Signaturzertifikat der QES");
    F_013.start("Beim Mapping von KBV_PR_FOR_Practitioner soll bei identifier:Telematik-ID die Telematik-ID "
                "des Arztes gesetzt werden welche auch im Signaturzertifikat der QES angegeben ist (siehe auch "
                "A_25946 - E-Rezept-Fachdienst - ePA Mapping)");
    addTelematikIdentifier(transformedPractitioner, telematikIdFromQes.id(), true);
    addMetaTagOriginLdap(transformedPractitioner);
    F_013.finish();
    A_25946.finish();
    parameters.setPractitioner(transformedPractitioner);

    parameters.removeEmptyObjectsAndArrays();
    return parameters;
}

model::EPAOpProvideDispensationERPInputParameters Epa4AllTransformer::transformMedicationDispenseWithKBVMedication(
    const model::Bundle& medicationDispenseBundle, model::PrescriptionId prescriptionId,
    const model::Timestamp& authoredOn, const model::TelematikId& actorTelematikId,
    const std::string& actorOrganizationName, const std::string& organizationProfessionOid)
{
    auto parameters = transformMedicationDispenseCommon(prescriptionId, authoredOn, actorTelematikId,
                                                        actorOrganizationName, organizationProfessionOid);

    for (size_t i = 0, end = medicationDispenseBundle.getResourceCount(); i < end; ++i)
    {
        const auto sourceResource = model::UnspecifiedResource::fromJson(medicationDispenseBundle.getResource(i));
        auto containedMedicationOpt = sourceResource.containedResource<model::KbvMedicationGeneric>(0);
        Expect(containedMedicationOpt, "MedicationDispense without contained Medication");
        containedMedicationOpt->setId(Uuid{}.toString());

        auto transformedMedication = transformKbvMedication(*containedMedicationOpt);

        F_014.start("Eine Unterscheidung nach GKV und PKV im System findet beim Mapping nicht mehr statt.");
        std::vector<fhirtools::ValueMapping> dispenseValueMappings{
            {.from = "http://fhir.de/sid/pkv/kvid-10", .to = "http://fhir.de/sid/gkv/kvid-10"}};
        F_014.finish();
        std::vector<FixedValue> dispenseFixedValues{
            {.ptr = rapidjson::Pointer{"/medicationReference/reference"},
             .value = "Medication/" + std::string{*containedMedicationOpt->getId()}}};
        auto transformedMedicationDispense =
            transformResource(EpaMedicationDispenseProfile, sourceResource, dispenseValueMappings, dispenseFixedValues);
        remove(transformedMedicationDispense,
               rapidjson::Pointer{model::resource::ElementName::path(model::resource::elements::contained)});
        parameters.addMedicationAndDispense(transformedMedicationDispense, transformedMedication);
    }

    parameters.removeEmptyObjectsAndArrays();
    return parameters;
}

model::EPAOpProvideDispensationERPInputParameters Epa4AllTransformer::transformMedicationDispense(
    const model::Bundle& medicationDispenseBundle, const model::PrescriptionId& prescriptionId,
    const model::Timestamp& authoredOn, const model::TelematikId& actorTelematikId,
    const std::string& actorOrganizationName, const std::string& organizationProfessionOid)
{
    auto parameters = transformMedicationDispenseCommon(prescriptionId, authoredOn, actorTelematikId,
                                                        actorOrganizationName, organizationProfessionOid);

    for (auto& medicationDispense : medicationDispenseBundle.getResourcesByType<model::MedicationDispense>())
    {
        medicationDispense.setMetaProfile0(to_string(EpaMedicationDispenseProfile));
        Uuid medicationReference{medicationDispense.medicationReference()};
        if (medicationReference.isValidIheUuid())
        {
            medicationDispense.setMedicationReference("Medication/" + medicationReference.toString());
        }
        parameters.addMedicationDispense(medicationDispense.jsonDocument());
    }
    for (auto& medication : medicationDispenseBundle.getResourcesByType<model::UnspecifiedResource>("Medication"))
    {
        medication.setMetaProfile0(to_string(EpaMedicationProfile));
        parameters.addMedication(medication.jsonDocument());
    }

    parameters.removeEmptyObjectsAndArrays();
    return parameters;
}

std::string Epa4AllTransformer::buildPractitionerName(const rapidjson::Value& humanNameElement)
{
    static const rapidjson::Pointer familyPtr{"/family"};
    const auto familyOpt = model::NumberAsStringParserDocument::getOptionalStringValue(humanNameElement, familyPtr);
    Expect(familyOpt.has_value(), "HumanName.family is mandatory in KBV_PR_Base_Datatype_Name");

    std::optional<std::string> familyNamenszusatz;
    std::string familyNachname;
    std::optional<std::string> familyVorsatzwort;

    static const rapidjson::Pointer familyExtensionArrayPtr{"/_family/extension"};
    const auto* familyExtensionArray = familyExtensionArrayPtr.Get(humanNameElement);
    if (familyExtensionArray)
    {
        static const rapidjson::Pointer urlPtr{"/url"};
        static const rapidjson::Pointer valueStringPtr{"/valueString"};
        if (const auto* namenszusatz = model::NumberAsStringParserDocument::findMemberInArray(
                familyExtensionArray, urlPtr, "http://fhir.de/StructureDefinition/humanname-namenszusatz"))
        {
            familyNamenszusatz =
                model::NumberAsStringParserDocument::getOptionalStringValue(*namenszusatz, valueStringPtr);
        }
        const auto* nachname = model::NumberAsStringParserDocument::findMemberInArray(
            familyExtensionArray, urlPtr, "http://hl7.org/fhir/StructureDefinition/humanname-own-name");
        Expect(nachname, "Practitioner.name:name.family.extension:nachname.value[x]:valueString is mandatory");
        auto nachNameValue = model::NumberAsStringParserDocument::getOptionalStringValue(*nachname, valueStringPtr);
        Expect(nachNameValue, "Practitioner.name:name.family.extension:nachname.value[x]:valueString is mandatory");
        familyNachname = *nachNameValue;

        if (const auto* vorsatzwort = model::NumberAsStringParserDocument::findMemberInArray(
                familyExtensionArray, urlPtr, "http://hl7.org/fhir/StructureDefinition/humanname-own-prefix"))
        {
            familyVorsatzwort =
                model::NumberAsStringParserDocument::getOptionalStringValue(*vorsatzwort, valueStringPtr);
        }
    }

    static const rapidjson::Pointer givenPtr{"/given/0"};
    const auto givenOpt = model::NumberAsStringParserDocument::getOptionalStringValue(humanNameElement, givenPtr);
    Expect(givenOpt.has_value(), "HumanName.given is mandatory in KBV_PR_Base_Datatype_Name");

    static const rapidjson::Pointer prefixPtr{"/prefix/0"};
    const auto prefixOpt = model::NumberAsStringParserDocument::getOptionalStringValue(humanNameElement, prefixPtr);

    std::ostringstream oss;
    if (prefixOpt)
    {
        oss << "AC " << *prefixOpt << " ";
    }
    oss << *givenOpt;
    if (familyVorsatzwort)
    {
        oss << " " << *familyVorsatzwort;
    }
    oss << " " << familyNachname;
    if (familyNamenszusatz)
    {
        oss << " " << *familyNamenszusatz;
    }
    return oss.str();
}

model::EPAOpProvideDispensationERPInputParameters Epa4AllTransformer::transformMedicationDispenseCommon(
    model::PrescriptionId prescriptionId, const model::Timestamp& authoredOn,
    const model::TelematikId& actorTelematikId, const std::string& actorOrganizationName,
    const std::string& organizationProfessionOid)
{
    model::EPAOpProvideDispensationERPInputParameters parameters;
    parameters.setId(Uuid{}.toString());
    parameters.setPrescriptionId(prescriptionId);
    F_001.start("set provide-dispensation.authoredOn to MedicationRequest.authoredOn");
    parameters.setAuthoredOn(authoredOn);
    F_001.finish();

    A_25947.start("identifier:TelematikID and name from JWT");
    F_006a.start("Defaultwerte bei Organization");
    model::OrganizationDirectory organization{actorTelematikId,
                                              model::Coding{model::resource::naming_system::organizationProfessionOid,
                                                            profession_oid::oid_oeffentliche_apotheke},
                                              actorOrganizationName, organizationProfessionOid};
    F_006a.finish();
    A_25947.finish();
    parameters.setOrganization(organization.jsonDocument());
    return parameters;
}

model::NumberAsStringParserDocument
Epa4AllTransformer::transformKbvMedication(const model::KbvMedicationGeneric& kbvMedication)
{
    F_009.start("extension mapping medication-ingredient-amount-extension");
    F_005.start("Medication.ingredient.strength.extension:amountText");
    F_005.start("Medication.extension:packaging");
    std::vector<fhirtools::ValueMapping> medicationValueMappings = {
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/drug-category-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_CompoundingInstruction",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/"
               "medication-manufacturing-instructions-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Vaccine",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-id-vaccine-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Packaging",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-formulation-packaging-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Form",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/"
               "medication-ingredient-darreichungsform-extension"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Ingredient_Amount",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-ingredient-amount-extension"},
        {.from = "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category",
         .to = "https://gematik.de/fhir/epa-medication/CodeSystem/epa-drug-category-cs"},
    };
    if (kbvMedication.getProfile() == model::ProfileType::KBV_PR_ERP_Medication_Compounding)
    {
        F_005.start("Medication.amount.numerator.extension:totalQuantity");
        medicationValueMappings.emplace_back(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize",
            "https://gematik.de/fhir/epa-medication/StructureDefinition/"
            "medication-total-quantity-formulation-extension");
        F_005.finish();
        F_005.start("Medication.extension:manufacturingInstructions");
        medicationValueMappings.emplace_back(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_CompoundingInstruction",
            "https://gematik.de/fhir/epa-medication/StructureDefinition/"
            "medication-manufacturing-instructions-extension");
        F_005.finish();
    }
    else
    {
        F_005.start("Medication.amount.numerator.extension:packagingSize");
        medicationValueMappings.emplace_back(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_PackagingSize",
            "https://gematik.de/fhir/epa-medication/StructureDefinition/medication-packaging-size-extension");
        F_005.finish();
    }

    // requirements are implicitly implemented by generic tranformer:
    F_008.start(
        "Wenn in der KBV Medication bei Medication.ingredient.strength bei numerator und denominator system und code "
        "fehlen, muss stattdessen die data-absent-reason Extension in der EPA Medication gesetzt werden");
    F_009.start("Um die Pflichtfeld Constraints im EPA Medication Profil zu erfüllen müssen außerdem "
                "für numerator und denomitator die data-absent-reason Extension gesetzt werden");
    auto transformedMedication = transformResource(EpaMedicationProfile, kbvMedication, medicationValueMappings, {});
    F_015.start("Medication.code.coding allowlist");
    removeMedicationCodeCodingsByAllowlist(transformedMedication);
    F_015.finish();
    if (kbvMedication.getProfile() == model::ProfileType::KBV_PR_ERP_Medication_Compounding)
    {
        F_017.start("convert Medication.ingredient to Medication.contained");
        const auto kbvMedicationCompounding = model::KbvMedicationCompounding::fromJson(kbvMedication.jsonDocument());
        convertPZNIngredients(transformedMedication, kbvMedicationCompounding);
        F_017.finish();
    }
    return transformedMedication;
}

void Epa4AllTransformer::convertPZNIngredients(model::NumberAsStringParserDocument& transformedMedication,
                                               const model::KbvMedicationCompounding& kbvMedicationCompounding)
{
    static const rapidjson::Pointer ingredientArrayPointer(model::resource::ElementName::path("ingredient"));
    const auto* kbvIngredients = kbvMedicationCompounding.ingredientArray();
    Expect(kbvIngredients && kbvIngredients->IsArray(),
           "Ingredients array in KBV_PR_ERP_Medication_Compounding defective");
    auto* epaIngredients = ingredientArrayPointer.Get(transformedMedication);
    Expect(epaIngredients && epaIngredients->IsArray(), "Ingredients array in transformed epa-medication defective");
    Expect(kbvIngredients->Size() == epaIngredients->Size(),
           "Ingredient arrays in source and target have different sizes after transformation");

    for (rapidjson::SizeType i = 0; i < epaIngredients->Size(); ++i)
    {
        const auto& kbvIngredient = kbvIngredients->GetArray()[i];
        auto& epaIngredient = epaIngredients->GetArray()[i];
        static const rapidjson::Pointer systemPointer(
            model::resource::ElementName::path("itemCodeableConcept", "coding", 0, "system"));
        static const rapidjson::Pointer codePointer(
            model::resource::ElementName::path("itemCodeableConcept", "coding", 0, "code"));
        const auto* kbvIngredientSystem = systemPointer.Get(kbvIngredient);
        const auto* kbvIngredientCode = codePointer.Get(kbvIngredient);
        if (kbvIngredientSystem && kbvIngredientCode)
        {
            static const rapidjson::Pointer textPointer(
                model::resource::ElementName::path("itemCodeableConcept", "text"));
            const auto* kbvIngredientText = textPointer.Get(kbvIngredient);
            const auto* epaIngredientText = textPointer.Get(epaIngredient);
            Expect(kbvIngredientText != nullptr && epaIngredientText != nullptr,
                   "itemCodeableConcept.text is mandatory but missing");
            const auto kbvIngredientTextString =
                model::NumberAsStringParserDocument::getStringValueFromValue(kbvIngredientText);
            const auto epaIngredientTextString =
                model::NumberAsStringParserDocument::getStringValueFromValue(epaIngredientText);
            Expect(kbvIngredientTextString == epaIngredientTextString,
                   "Difference in value of itemCodeableConcept.text between KBV and transformed.");
            Expect(kbvIngredientSystem->IsString(), "ingredient.itemCodeableConcept.coding.system defective");
            Expect(kbvIngredientCode->IsString(), "ingredient.itemCodeableConcept.coding.code defective");
            Expect(model::NumberAsStringParserDocument::getStringValueFromValue(kbvIngredientSystem) ==
                       model::resource::code_system::pzn,
                   "ingredient.itemCodeableConcept.coding.code expected to be http://fhir.de/CodeSystem/ifa/pzn");
            convertPZNIngredient(
                transformedMedication, epaIngredient,
                model::Pzn{model::NumberAsStringParserDocument::getStringValueFromValue(kbvIngredientCode)},
                kbvIngredientTextString);
            remove(epaIngredient, rapidjson::Pointer{model::resource::ElementName::path("itemCodeableConcept")});
        }
    }
}

void Epa4AllTransformer::convertPZNIngredient(model::NumberAsStringParserDocument& transformedMedication,
                                              rapidjson::Value& epaIngredient, model::Pzn&& pzn, std::string_view text)
{
    model::EPAMedicationPZNIngredient containedMedication{std::move(pzn), text};

    static const rapidjson::Pointer itemReferencePointer(
        model::resource::ElementName::path("itemReference", "reference"));
    const auto containedMedicationIdOpt = containedMedication.getId();
    Expect(containedMedicationIdOpt.has_value(), "missing ID in contained medication");
    transformedMedication.setKeyValue(epaIngredient, itemReferencePointer,
                                      "#" + std::string{*containedMedicationIdOpt});

    static const rapidjson::Pointer containedArrayPointer(
        model::resource::ElementName::path(model::resource::elements::contained));
    rapidjson::Value rjObject{rapidjson::kObjectType};
    // Need to have the right Allocator in the object, cannot simply add containedMedication.jsonDocument() to array:
    rjObject.CopyFrom(containedMedication.jsonDocument(), transformedMedication.GetAllocator());
    transformedMedication.addToArray(containedArrayPointer, std::move(rjObject));
}

model::NumberAsStringParserDocument Epa4AllTransformer::transformResource(
    const fhirtools::DefinitionKey& targetProfileKey, const model::FhirResourceBase& sourceResource,
    const std::vector<fhirtools::ValueMapping>& valueMappings, const std::vector<FixedValue>& fixedValues)
{
    auto repoView = getRepo(targetProfileKey);
    const auto* targetProfile = repoView->findStructure(targetProfileKey);
    Expect(targetProfile, "profile not found: " + to_string(targetProfileKey));

    model::NumberAsStringParserDocument targetResource;
    targetResource.CopyFrom(sourceResource.jsonDocument(), targetResource.GetAllocator());

    for (const auto& fixedValue : fixedValues)
    {
        targetResource.setValue(fixedValue.ptr, fixedValue.value);
    }

    const auto elementId = targetProfile->typeId();
    const auto baseTargetProfile = fhirtools::ProfiledElementTypeInfo{repoView, elementId};

    auto erpElement = std::make_shared<ErpElement>(repoView, std::weak_ptr<const fhirtools::Element>{},
                                                   baseTargetProfile, &targetResource, &targetResource, nullptr);

    fhirtools::ResourceProfileTransformer::Options options;
    options.removeUnknownExtensionsFromOpenSlicing = true;
    fhirtools::ResourceProfileTransformer resourceProfileTransformer(repoView.get(), nullptr, targetProfile, options);
    resourceProfileTransformer.map(valueMappings);
    resourceProfileTransformer.map(
        {.from = to_string(sourceResource.getDefinitionKeyFromMeta()), .to = to_string(targetProfileKey)});

    auto result = resourceProfileTransformer.applyMappings(*erpElement);
    Expect(result.success, result.summary());
    erpElement = std::make_shared<ErpElement>(repoView, std::weak_ptr<const fhirtools::Element>{}, baseTargetProfile,
                                              &targetResource, &targetResource, nullptr);

    A_25948.start("generic mapping of fhir resources");
    A_25949.start("generic extension takeover");
    result = resourceProfileTransformer.transform(*erpElement, targetProfileKey);
    // we possibly need to do two rounds, because transformation can unmask other validation errors.
    // e.g. adding data-absent-reason to numerator unmasks the constraint violation rat-1 (missing denominator)
    erpElement = std::make_shared<ErpElement>(repoView, std::weak_ptr<const fhirtools::Element>{}, baseTargetProfile,
                                              &targetResource, &targetResource, nullptr);
    result.merge(resourceProfileTransformer.transform(*erpElement, targetProfileKey));
    A_25949.finish();
    A_25948.finish();

    Expect(result.success, result.summary());
    return targetResource;
}

void Epa4AllTransformer::removeMedicationCodeCodingsByAllowlist(rapidjson::Value& medicationResource)
{
    static const std::unordered_set<std::string_view> allowList{
        model::resource::code_system::pzn, model::resource::code_system::atc, model::resource::code_system::ask,
        model::resource::code_system::sct};
    static const rapidjson::Pointer codeCodingArrayPtr{
        model::resource::ElementName::path(model::resource::elements::code, model::resource::elements::coding)};
    static const rapidjson::Pointer systemPtr{model::resource::ElementName::path(model::resource::elements::system)};
    auto* codingArray = codeCodingArrayPtr.Get(medicationResource);
    if (codingArray && codingArray->IsArray() && codingArray->Size() > 0)
    {
        for (auto* it = codingArray->Begin(); it != codingArray->End();)
        {
            const auto system = model::NumberAsStringParserDocument::getOptionalStringValue(*it, systemPtr);
            Expect(system, "missing system in Medication.code.coding");
            if (! allowList.contains(*system))
            {
                TVLOG(1) << "removing Medication.code.coding entry with system not on allowlist: " << *system;
                it = codingArray->Erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (codingArray->Empty())
        {
            codeCodingArrayPtr.Erase(medicationResource);
            static const rapidjson::Pointer codePtr{
                model::resource::ElementName::path(model::resource::elements::code)};
            const auto* codeElement = codePtr.Get(medicationResource);
            if (codeElement && codeElement->MemberCount() == 0)
            {
                codePtr.Erase(medicationResource);
            }
        }
    }
}

void Epa4AllTransformer::remove(rapidjson::Value& from, const rapidjson::Pointer& toBeRemoved)
{
    toBeRemoved.Erase(from);
}

void Epa4AllTransformer::checkSetAbsentReason(model::NumberAsStringParserDocument& document,
                                              const std::string& memberPath)
{
    if (! rapidjson::Pointer(memberPath).Get(document))
    {
        document.setValue(rapidjson::Pointer(memberPath + "/extension/0"), model::DataAbsentReason().jsonDocument());
    }
}

void Epa4AllTransformer::addToKeyValueArray(model::NumberAsStringParserDocument& document,
                                            const rapidjson::Pointer& arrayPointer, const rapidjson::Pointer& keyPtr,
                                            const std::string_view key, const rapidjson::Pointer& valuePtr,
                                            const std::string_view value, bool overwrite)
{
    if (auto* entry = document.findMemberInArray(arrayPointer, keyPtr, key))
    {
        if (overwrite)
        {
            document.setKeyValue(*entry, valuePtr, value);
        }
    }
    else
    {
        auto newEntry = model::NumberAsStringParserDocument::createEmptyObject();
        document.setKeyValue(newEntry, keyPtr, key);
        document.setKeyValue(newEntry, valuePtr, value);
        document.addToArray(arrayPointer, std::move(newEntry));
    }
}

void Epa4AllTransformer::addTelematikIdentifier(model::NumberAsStringParserDocument& document,
                                                const std::string_view& telematikIdFromQes, bool overwrite)
{
    addToKeyValueArray(document, rapidjson::Pointer{"/identifier"}, rapidjson::Pointer{"/system"},
                       "https://gematik.de/fhir/sid/telematik-id", rapidjson::Pointer{"/value"}, telematikIdFromQes,
                       overwrite);
}

void Epa4AllTransformer::addMetaTagOriginLdap(model::NumberAsStringParserDocument& document)
{
    addToKeyValueArray(document, rapidjson::Pointer{"/meta/tag"}, rapidjson::Pointer{"/system"},
                       model::resource::code_system::origin, rapidjson::Pointer{"/code"}, "ldap", false);
}

void Epa4AllTransformer::addTypeProfessionOid(model::NumberAsStringParserDocument& document,
                                              const std::string& organizationProfessionOid)
{
    addToKeyValueArray(document, rapidjson::Pointer{"/type"}, rapidjson::Pointer{"/coding/0/system"},
                       "https://gematik.de/fhir/directory/CodeSystem/OrganizationProfessionOID",
                       rapidjson::Pointer{"/coding/0/code"}, organizationProfessionOid, false);
}

std::shared_ptr<fhirtools::FhirStructureRepository>
Epa4AllTransformer::getRepo(const fhirtools::DefinitionKey& profileKey)
{
    auto viewList = Fhir::instance().structureRepository(model::Timestamp::now());
    auto repoView = viewList.match(std::addressof(Fhir::instance().backend()), profileKey.url, *profileKey.version);
    Expect(repoView, "No repository view for " + to_string(profileKey));
    return repoView;
}
