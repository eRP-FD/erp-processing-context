/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/Epa4AllTransformer.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/model/DataAbsentReason.hxx"
#include "exporter/model/OrganizationDirectory.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "model/EpaMedicationPznIngredient.hxx"
#include "model/EpaMedicationTypeExtension.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/KbvComposition.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/KbvMedicationCompounding.hxx"
#include "shared/model/KbvMedicationPzn.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/KbvOrganization.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/Patient.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/model/ResourceNames.hxx"

#include <unordered_set>

namespace
{
const fhirtools::DefinitionKey EpaMedicationRequestProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-request"};
const fhirtools::DefinitionKey EpaMedicationProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication"};
const fhirtools::DefinitionKey EpaOrganizationProfile{
    "https://gematik.de/fhir/directory/StructureDefinition/OrganizationDirectory"};
const fhirtools::DefinitionKey EpaPractitionerProfile{
    "https://gematik.de/fhir/directory/StructureDefinition/PractitionerDirectory"};
const fhirtools::DefinitionKey BundleProfile{"http://hl7.org/fhir/StructureDefinition/Bundle|4.0.1"};
const fhirtools::DefinitionKey EpaMedicationDispenseProfile{
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-dispense"};
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

    auto medicationRequest = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    const auto& kbvMedication = kbvBundle.getUniqueResourceByType<model::KbvMedicationGeneric>();
    Expect(kbvMedication.getId().has_value(), "Medication resource without ID");
    F_001.start("set provide-prescription.authoredOn to MedicationRequest.authoredOn");
    parameters.setAuthoredOn(medicationRequest.authoredOn());
    F_001.finish();

    if (medicationRequest.getProfileVersionChecked() < model::version::KBV_1_4)
    {
        medicationRequest.movePatientInstructionToText();
    }

    const auto& patient = kbvBundle.getUniqueResourceByType<model::Patient>();

    // medicationRequest
    const Uuid medicationRequestId{};
    F_004.start("mapping of MedicationRequest.extension:Mehrfachverordnung to "
                "MedicationRequest.extension:multiplePrescription");
    const std::vector<fhirtools::ValueMapping> medicationRequestValueMappings = {
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/multiple-prescription-extension"},
        {.from = "Kennzeichen", .to = "indicator", .restrictFieldName = "url"},
        {.from = "Nummerierung", .to = "counter", .restrictFieldName = "url"},
        {.from = "Zeitraum", .to = "period", .restrictFieldName = "url"},
        {.from = "ID", .to = "id", .restrictFieldName = "url"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_SER",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/indicator-ser-extension",
         .restrictFieldName = "url"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Narcotic",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/narcotics-extension",
         .restrictFieldName = "url"},
        {.from = "ErgaenzendeAngabenSubstitutionsmittel",
         .to = "additional-information-substitutes",
         .restrictFieldName = "url"},
        {.from = "BtM-Sonderkennzeichen", .to = "narcotics-markings", .restrictFieldName = "url"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Patient_ID",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/patient-id-extension",
         .restrictFieldName = "url"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Prescriber_ID",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/prescriber-id-extension",
         .restrictFieldName = "url"},
        {.from = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Teratogenic",
         .to = "https://gematik.de/fhir/epa-medication/StructureDefinition/teratogenic-extension",
         .restrictFieldName = "url"},
        {.from = "GebaerfaehigeFrau", .to = "childbearing-potential", .restrictFieldName = "url"},
        {.from = "ErklaerungSachkenntnis", .to = "declaration-of-expertise", .restrictFieldName = "url"},
        {.from = "AushaendigungInformationsmaterialien",
         .to = "hand-out-information-material",
         .restrictFieldName = "url"},
        {.from = "Off-Label", .to = "off-label", .restrictFieldName = "url"},
        {.from = "EinhaltungSicherheitsmassnahmen", .to = "security-compliance", .restrictFieldName = "url"},
    };
    A_25946.start("KBV_PR_ERP_Prescription: Setzen des Pattern \"filler-order\" für .intent");
    F_010.start("Das Befüllen des MedicationRequest.subject");
    F_002.start("rewrite /medicationReference/reference");
    F_021.start("Add code and system defaults");
    const std::vector<FixedValue> medicationRequestFixedValues{
        {.ptr = rapidjson::Pointer{"/dispenseRequest/quantity/code"}, .value = "{Package}"},
        {.ptr = rapidjson::Pointer{"/dispenseRequest/quantity/system"}, .value = "http://unitsofmeasure.org"},
        {.ptr = rapidjson::Pointer{"/intent"}, .value = "filler-order"},
        {.ptr = rapidjson::Pointer{"/id"}, .value = medicationRequestId.toString()},
        {.ptr = rapidjson::Pointer{"/subject/identifier/system"},
         .value = std::string{model::resource::naming_system::gkvKvid10}},
        {.ptr = rapidjson::Pointer{"/subject/identifier/value"}, .value = patient.kvnr().id()},
        {.ptr = rapidjson::Pointer{"/medicationReference/reference"},
         .value = "Medication/" + std::string{*kbvMedication.getId()}}};
    F_021.finish();
    F_002.finish();
    F_010.finish();
    A_25946.finish();
    auto transformedMedicationRequest =
        transformResource(EpaMedicationRequestProfile, medicationRequest,
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

    // organization
    const std::vector<fhirtools::ValueMapping> organizationValueMappings = {};
    F_006b.start("organizationName → aus dem ACCESS_TOKEN der Anfrage");
    const std::vector<FixedValue> organizationFixedValues{
        {.ptr = rapidjson::Pointer{"/name"}, .value = std::string{organizationNameFromJwt}}};

    F_007.start(
        "Beim Mapping der Organization bei providePrescription (siehe 006b) sollen die Extensions in das OpenSlicing "
        "des Organization-Profils des VZD übernommen werden, auch wenn diese nicht explizit dort modelliert sind.");
    auto transformedOrganization = transformResource(EpaOrganizationProfile,
                                                     kbvBundle.getUniqueResourceByType<model::KbvOrganization>(),
                                                     organizationValueMappings, organizationFixedValues);
    // according to https://gematik.github.io/api-erp/erp_epa_mapping_details/2026_07_01/mapping-prescription-organization.html
    remove(transformedOrganization, rapidjson::Pointer{"/identifier"});
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
    const std::vector<fhirtools::ValueMapping> practitionerValueMappings = {};
    const std::vector<FixedValue> practitionerFixedValues{};
    const auto* backend = std::addressof(Fhir::instance().backend());
    auto repoView = getRepo(BundleProfile);
    const auto* bundleProfile = repoView->findStructure(BundleProfile);
    Expect(bundleProfile, "profile not found: " + to_string(BundleProfile));
    auto erpElement = std::make_shared<ErpElement>(backend, std::weak_ptr<const fhirtools::Element>{},
                                                   fhirtools::ProfiledElementTypeInfo{*bundleProfile},
                                                   &kbvBundle.jsonDocument(), nullptr);
    F_016.start(
        "resolve the Composition author reference, as there might be 1..2 practitioners (unordered) in the bundle.");
    const std::string expression =
        "Bundle.entry.resource.ofType(Composition).author.where(type='Practitioner').resolve()";
    auto resolveReferenceExpression = fhirtools::FhirPathParser::parse(backend, expression);
    Expect3(resolveReferenceExpression, "could not parse resolveReferenceExpression", std::logic_error);
    auto resolvedReferenceCollection =
        resolveReferenceExpression->eval(fhirtools::EvaluationContext{repoView, erpElement}).collection;
    Expect(resolvedReferenceCollection.size() == 1, expression + " could not be resolved");
    auto resolvedRef = resolvedReferenceCollection.single();
    Expect(resolvedRef != nullptr, expression + " could not be resolved");
    F_016.finish();
    auto practitioner =
        model::UnspecifiedResource::fromJson(*std::dynamic_pointer_cast<const ErpElement>(resolvedRef)->asJson());
    const auto* humanNameElement = rapidjson::Pointer{"/name/0"}.Get(practitioner.jsonDocument());
    Expect(humanNameElement, "name field missing in practitioner");
    practitioner.setValue(rapidjson::Pointer{"/name/0/text"}, buildPractitionerName(*humanNameElement));
    auto transformedPractitioner = transformResource(EpaPractitionerProfile, practitioner,
                                                     practitionerValueMappings, practitionerFixedValues);
    A_25946.start("Überschreiben/Setzen der \"identifier:Telematik-ID\" des Arztes aus dem Signaturzertifikat der QES");
    F_013.start("Beim Mapping von KBV_PR_FOR_Practitioner soll bei identifier:Telematik-ID die Telematik-ID "
                "des Arztes gesetzt werden welche auch im Signaturzertifikat der QES angegeben ist (siehe auch "
                "A_25946 - E-Rezept-Fachdienst - ePA Mapping)");
    addTelematikIdentifier(transformedPractitioner, telematikIdFromQes.id(), true);
    addMetaTagOriginLdap(transformedPractitioner);
    F_013.finish();
    A_25946.finish();

    F_019.start("Kein mapping von Practitioner.qualification");
    remove(transformedPractitioner, rapidjson::Pointer("/qualification"));
    F_019.finish();

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
        auto sourceResource = model::MedicationDispense::fromJson(medicationDispenseBundle.getResource(i));
        auto containedMedicationOpt = sourceResource.containedResource<model::KbvMedicationGeneric>(0);
        Expect(containedMedicationOpt, "MedicationDispense without contained Medication");
        containedMedicationOpt->setId(Uuid{}.toString());

        auto transformedMedication = transformKbvMedication(*containedMedicationOpt);

        F_014.start("Eine Unterscheidung nach GKV und PKV im System findet beim Mapping nicht mehr statt.");
        const std::vector<fhirtools::ValueMapping> dispenseValueMappings{
            {.from = "http://fhir.de/sid/pkv/kvid-10", .to = "http://fhir.de/sid/gkv/kvid-10"}};
        F_014.finish();
        const std::vector<FixedValue> dispenseFixedValues{
            {.ptr = rapidjson::Pointer{"/medicationReference/reference"},
             .value = "Medication/" + std::string{*containedMedicationOpt->getId()}}};
        if (sourceResource.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
        {
            sourceResource.moveAppendPatientInstructionToText();
        }
        auto transformedMedicationDispense =
            transformResource(EpaMedicationDispenseProfile, sourceResource,
                              dispenseValueMappings, dispenseFixedValues);
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
        const Uuid medicationReference{medicationDispense.medicationReference()};
        if (medicationReference.isValidIheUuid())
        {
            medicationDispense.setMedicationReference("Medication/" + medicationReference.toString());
        }
        if (medicationDispense.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
        {
            medicationDispense.moveAppendPatientInstructionToText();
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
    F_011.start("Der Practitioner.name.text ist ein Pflichtfeld und muss aus den Namensinformationen erzeugt werden");
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
    model::OrganizationDirectory organization{
        actorTelematikId,
        model::Coding{model::resource::naming_system::organizationProfessionOid,
                      profession_oid::oid_oeffentliche_apotheke},
        actorOrganizationName, organizationProfessionOid};
    F_006a.finish();
    A_25947.finish();
    // Set profile without version
    organization.setMetaProfile0(to_string(EpaOrganizationProfile));
    parameters.setOrganization(organization.jsonDocument());
    return parameters;
}

model::NumberAsStringParserDocument
Epa4AllTransformer::transformKbvMedication(const model::KbvMedicationGeneric& kbvMedication)
{
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

    auto transformedMedication =
        transformResource(EpaMedicationProfile, kbvMedication, medicationValueMappings, {});
    F_015.start("Medication.code.coding allowlist");
    removeMedicationCodeCodingsByAllowlist(transformedMedication);
    F_015.finish();
    switch (kbvMedication.getProfile())
    {
        case model::ProfileType::KBV_PR_ERP_Medication_Compounding: {
            F_017.start("convert Medication.ingredient to Medication.contained");
            const auto kbvMedicationCompounding =
                model::KbvMedicationCompounding::fromJson(kbvMedication.jsonDocument());
            convertPZNIngredients(transformedMedication, kbvMedicationCompounding);
            F_017.finish();
            F_020.start("5.: Rezeptur ohne PZNs in Rezepturbestandteilen");
            F_020.start("6.: Rezeptur mit PZNs in Rezepturbestandteilen");
            addMedicationTypeExtension(
                transformedMedication,
                model::EPAMedicationTypeExtension{model::EPAMedicationTypeExtension::Type::ExtemporaneousPreparation});
            break;
        }
        case model::ProfileType::KBV_PR_ERP_Medication_FreeText:
            F_020.start("4.: Freitextverordnung (Extension nicht setzen)");
            break;
        case model::ProfileType::KBV_PR_ERP_Medication_Ingredient:
            F_020.start("3.: Wirkstoffverordnung");
            addMedicationTypeExtension(
                transformedMedication,
                model::EPAMedicationTypeExtension{model::EPAMedicationTypeExtension::Type::MedicinalProductPackage});
            break;
        case model::ProfileType::KBV_PR_ERP_Medication_PZN: {
            const auto kbvMedicationPzn = model::KbvMedicationPzn::fromJson(kbvMedication.jsonDocument());
            if (kbvMedicationPzn.isKPG())
            {
                F_020.start("2.: PZN Verordnung einer Kombipackung (Extension nicht setzen)");
            }
            else
            {
                F_020.start("1.: PZN Verordnung");
                addMedicationTypeExtension(transformedMedication,
                                           model::EPAMedicationTypeExtension{
                                               model::EPAMedicationTypeExtension::Type::MedicinalProductPackage});
            }
            break;
        }
        default:
            break;
    }
    F_020.finish();
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
                                              rapidjson::Value& epaIngredient, const model::Pzn& pzn,
                                              std::string_view text)
{
    model::EPAMedicationPZNIngredient containedMedication{pzn, text};

    static const rapidjson::Pointer itemReferencePointer(
        model::resource::ElementName::path("itemReference", "reference"));
    const auto containedMedicationIdOpt = containedMedication.getId();
    Expect(containedMedicationIdOpt.has_value(), "missing ID in contained medication");
    transformedMedication.setKeyValue(epaIngredient, itemReferencePointer,
                                      "#" + std::string{*containedMedicationIdOpt});

    static const rapidjson::Pointer containedArrayPointer(
        model::resource::ElementName::path(model::resource::elements::contained));
    transformedMedication.copyToArray(containedArrayPointer, containedMedication.jsonDocument());
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

void Epa4AllTransformer::addMedicationTypeExtension(model::NumberAsStringParserDocument& document,
                                                    model::EPAMedicationTypeExtension&& typeExtension)
{
    document.copyToArray(rapidjson::Pointer{"/extension"}, std::move(typeExtension).jsonDocument());
}
