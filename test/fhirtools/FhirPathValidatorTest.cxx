/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/Bundle.hxx"
#include "erp/model/Communication.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <gtest/gtest.h>
#include <iostream>

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

using namespace fhirtools;

class FhirPathValidatorTest : public fhirtools::test::SampleValidationTest<FhirPathValidatorTest>
{
public:
    static std::list<std::filesystem::path> fileList()
    {
        return {
            // clang-format off
            "fhir/hl7.org/profiles-types.xml",
            "fhir/hl7.org/profiles-resources.xml",
            "fhir/hl7.org/extension-definitions.xml",
            "fhir/hl7.org/valuesets.xml",
            "fhir/hl7.org/v3-codesystems.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/AvailabilityStateExtension.StructureDefinition.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/BeneficiaryExtension.StructureDefinition.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-availabilityStatus.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-consenttype.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-documentType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-flowType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CourierService.StructureDefinition.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/erxDevice.StructureDefinition.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-AuditEvent-ErxAuditEvent.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Binary.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Bundle-GemErxBundle.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-ChargeItem-erxChargeItem.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationDispReq.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationInfoReq.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationRepresentative.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Composition-GemErxComposition2.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Consent-erxConsent.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-AcceptDate.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-ExpiryDate.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-InsuranceProvider.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-MarkingFlag.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-PackageQuanity.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-PrescriptionType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-SubstitutionAllowedType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-SupplyOptionsType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Identifier-PRESCRIPTIONID.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Identifier-TELEMATIKID.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-MedicationDispense-ERxMedicationDispense.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Organization-ERxOrganization.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Parameters-erxAcceptOperationOutParameters.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Signature-ErxSignature.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Task-ERxTask.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-AVAILABILITYSTATUS.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-CONSENTTYPE.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-DOCUMENTTYPE.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-FLOWTYPE.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-ORGANIZATIONTYPE.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-PERFORMERTYPE.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Payor_Type_KBV.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Qualification_Type.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Ursache_Type.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_Alternative_IK.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_Legal_basis.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_PKV_Tariff.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Coverage.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Organization.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Patient.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Practitioner.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_PractitionerRole.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_VS_FOR_Payor_type.xml",
            "fhir/profiles/kbv.ita.for-1.0.3/KBV_VS_FOR_Qualification_Type.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-arge-ik-klassifikation.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-ask.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-aufnahmeart.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-6-vorsatzworte.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-7-namenszusaetze.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-8-laenderkennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-alpha-id-se.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-atc.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-icd-10-gm.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-ops.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-edqm-doseform.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-identifier-type-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-ifa-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Codesystem-iso-3166-2-de-laendercodes.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-administrative-gender.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-diagnosis-role.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-identifier-type-v2.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-iso-3166.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-loinc-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-marital-status.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-observation-category.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-versicherungsart-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-destatis-ags.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-besondere-personengruppe.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-dmp-kennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-einlesedatum-karte.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-generation-egk.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-kostenerstattung.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-onlinepruefung-egk.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-ruhender-leistungsanspruch.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-versichertenart.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-version-vsdm.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-wop.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-zuzahlungsstatus.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-humanname-namenszusatz.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-ausrufezeichen.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-diagnosesicherheit.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-manifestationscode.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-primaercode.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-normgroesse.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-seitenlokalisation.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Observation-pflegegrad.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-address-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-chargeitem-ebm.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-alpha-id-se.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-ask.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-atc.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-icd10gm.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-ops.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-condition-de-icd10.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-gkv.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-sel.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-humanname-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-bsnr.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-efn.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-iknr.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-kvid10.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-kzva.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-lanr.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pid.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pkv.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pseudo-kvid.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-vknr.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-zanr.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Atemfrequenz.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Koerpertemperatur.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Kopfumfang.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Puls.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Sauerstoffsaettigung.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/UcumVitalsCommonDE.ValueSet.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.30--20180713131246.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.31--20180713132208.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.32--20180713132315.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.33--20180713132759.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.34--20180713132843.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.35--20181214170712.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.36--20181001183306.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.37--20190517134631.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.38--20180713162205.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.39--20180713132816.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.40--20180713132721.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.58--20180713162142.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.59--20180713162125.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ask.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-atc.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-Aufnahmeart.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-dimdi-icd10-gm.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-identifier-type-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ifa-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-iso-3166-2-de-laendercodes.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-kbv-s-icd-diagnosesicherheit.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ops-pflegegrad.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-versicherungsart-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-VitalSignDE-Koerpergewicht-Snomed.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-VitalSignDE-Koerpergroesse-Snomed.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Height_Loinc.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Length_UCUM.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Weight_Loinc.xml",
            "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Weight_UCUM.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseApgarScoreIdentifierGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseApgarScoreValueGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyHeightSnomedGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Body_Temperature_German.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Body_Temp_Unit_German.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightLoincGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightMethodSnomedGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightSnomedGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseDiagnosisSeverityGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseHeadCircumferenceBodySiteSnomedGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseHeadCircumferenceSnomedGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Heart_Rate_German.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseidentifiertype.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBasePractitionerFunctionGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseStageLifeGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseAdditionalComment.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseStageLife.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseTerminologyGerman.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Heart_Rate.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationApgarScore.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Blood_Pressure_New.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationBodyHeight.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Body_Temperature.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationBodyWeight.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationHeadCircumference.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Respiratory_Rate.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseOrganization.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBasePatient.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Peripheral_Oxygen_Saturation.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVPRBasePractitioner.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreIdentifierLoinc.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreIdentifierSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreValue.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyHeightLoinc.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyHeightSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Body_Temperature_Snomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Body_Temp_Unit.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightLoinc.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightMethodSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightUnit.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseDiagnoseSnomedCT.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBASEGemRSAnlage8.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseHeadCircumferenceBodySiteSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseHeadCircumferenceSnomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Heart_Rate_Snomed.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseIHEXDSAuthorSpecialityRestricted.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBasePractitionerSpecialityAddendum.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBasePractitionerSpeciality.xml",
            "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseStageLife.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Medication_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Section_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_StatusCoPayment.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Accident.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_BVG.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_DosageFlag.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_EmergencyServicesFee.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_CompoundingInstruction.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Ingredient_Amount.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Ingredient_Form.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Packaging.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Vaccine.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Multiple_Prescription.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_PracticeSupply_Payor.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_StatusCoPayment.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Bundle.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Composition.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_Compounding.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_FreeText.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_Ingredient.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_PZN.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_PracticeSupply.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Prescription.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_Accident_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_StatusCoPayment.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_WBO_V1.16.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM_V1.10.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DMP_V1.05.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM_V1.10.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DMP_V1.05.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml"
            // clang-format on

        };
    }
    ValidatorOptions validatorOptions() override
    {
        return {.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::enable};
    }
};


TEST_P(FhirPathValidatorTest, samples)
{
    ASSERT_NO_FATAL_FAILURE(run());
}
using Sample = fhirtools::test::Sample;
using Severity = fhirtools::Severity;

// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, FhirPathValidatorTest, ::testing::Values(
    Sample{"Bundle", "test/validation/xml/kbv/bundle/Bundle_valid_fromErp5822.xml", {
        // the following two are caused by missing values sets - ERP-10539
        {std::make_tuple(Severity::warning, "ValueSet contains no codes after expansion"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::warning, "Cannot validate ValueSet binding"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::error, R"-(reference is not literal or invalid but must be resolvable: {"type": "Device", "identifier": {"system": "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer", "value": "Y/400/1910/36/346"}})-"), "Bundle.entry[0].resource{Composition}.author[1]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[6].resource{Practitioner}.identifier[0].type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[4].resource{Organization}.identifier[0].type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://fhir.de/CodeSystem/ifa/pzn, it is empty or synthesized."), "Bundle.entry[2].resource{Medication}.code.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART, it has not been loaded."), "Bundle.entry[0].resource{Composition}.type.coding[0]"},
},{
        {"dom-6", "Bundle.entry[0].resource{Composition}"},
        {"dom-6", "Bundle.entry[1].resource{MedicationRequest}"},
        {"dom-6", "Bundle.entry[2].resource{Medication}"},
        {"dom-6", "Bundle.entry[3].resource{Coverage}"},
        {"dom-6", "Bundle.entry[4].resource{Organization}"},
        {"dom-6", "Bundle.entry[5].resource{Patient}"},
        {"dom-6", "Bundle.entry[6].resource{Practitioner}"},
        {"dom-6", "Bundle.entry[7].resource{PractitionerRole}"},
    }},
    Sample{"Communication", "test/fhir/conversion/communication_info_req.xml", {
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://fhir.de/CodeSystem/ifa/pzn, it is empty or synthesized."), "Communication.contained[0]{Medication}.code.coding[0]"},
    }, {
        {"dom-6", "Communication.contained[0]{Medication}"},
        {"dom-6", "Communication"},
    }},
    Sample{"Bundle", "test/EndpointHandlerTest/kbv_bundle_unexpected_extension.xml", {
        // the following two are caused by missing values sets - ERP-10539
        {std::make_tuple(Severity::warning, "ValueSet contains no codes after expansion"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::warning, "Cannot validate ValueSet binding"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "Bundle.entry[0].resource{Composition}.extension[1]"},
        {std::make_tuple(Severity::error, R"-(reference is not literal or invalid but must be resolvable: {"type": "Device", "identifier": {"system": "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer", "value": "X/000/1111/22/333"}})-"), "Bundle.entry[0].resource{Composition}.author[1]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[2].resource{Practitioner}.identifier[0].type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART, it has not been loaded."), "Bundle.entry[0].resource{Composition}.type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://fhir.de/CodeSystem/ifa/pzn, it is empty or synthesized."), "Bundle.entry[4].resource{Medication}.code.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[3].resource{Organization}.identifier[0].type.coding[0]"},
      },{
        {"dom-6", "Bundle.entry[0].resource{Composition}"},
        {"dom-6", "Bundle.entry[1].resource{Patient}"},
        {"dom-6", "Bundle.entry[2].resource{Practitioner}"},
        {"dom-6", "Bundle.entry[3].resource{Organization}"},
        {"dom-6", "Bundle.entry[4].resource{Medication}"},
        {"dom-6", "Bundle.entry[5].resource{Coverage}"},
        {"dom-6", "Bundle.entry[6].resource{MedicationRequest}"},
    }},
    Sample{"OperationOutcome", "test/fhir-path/samples/invalid_empty_value.xml", {
        {std::make_tuple(Severity::error, "Attribute value must not be empty"), "OperationOutcome.issue[0].diagnostics"}
    }, {
        {"dom-6", "OperationOutcome"}
    }}
));
// clang-format on


class FhirPathValidatorTestSamples : public ::fhirtools::test::SampleValidationTest<FhirPathValidatorTestSamples>
{

public:
    static std::list<std::filesystem::path> fileList()
    {
        auto result = SampleValidationTest::fileList();
        result.merge({"test/fhir-path/profiles/alternative_profiles.xml"});
        return result;
    }
    ValidatorOptions validatorOptions() override
    {
        auto opt = SampleValidationTest::validatorOptions();
        opt.reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::enable;
        return opt;
    }
};


TEST_P(FhirPathValidatorTestSamples, run)//NOLINT
{
    ASSERT_NO_FATAL_FAILURE(run());
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(samples, FhirPathValidatorTestSamples,  ::testing::Values(
        Sample{"AlternativeProfiles", "test/fhir-path/samples/valid_AlternativeProfiles1.json", { } },
        Sample{"AlternativeProfiles", "test/fhir-path/samples/valid_AlternativeProfiles2.json", { } },
        Sample{"AlternativeProfiles", "test/fhir-path/samples/invalid_AlternativeProfiles1.json", {}, {
            {"Alternative1", "AlternativeProfiles.resource[1]{Resource}"},
            {"Alternative2", "AlternativeProfiles.resource[1]{Resource}"},
        }},
        Sample{"AlternativeProfiles", "test/fhir-path/samples/invalid_AlternativeProfiles2.json", {}, {
            {"Alternative1", "AlternativeProfiles.resource[0]{Resource}"},
            {"Alternative2", "AlternativeProfiles.resource[0]{Resource}"},
        }},
        Sample{"Bundle", "test/fhir-path/samples/ERP-11476-contentReferenceTypedField.xml"}
));
// clang-format on

TEST_F(FhirPathValidatorTest, OperationalValueSets)
{
    {
        const auto* valueSet = repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Accident_Type", "1.0.2");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(valueSet->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_FALSE(valueSet->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(valueSet->containsCode("4", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Medication_Category", "1.0.2");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
    }
    {
        const auto* valueSet = repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_ERP_StatusCoPayment", "1.0.2");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("0", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_StatusCoPayment"));
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_StatusCoPayment"));
        EXPECT_TRUE(valueSet->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_StatusCoPayment"));
    }
    {
        const auto* valueSet = repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Payor_type", "1.0.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("GKV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(valueSet->containsCode("PKV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(valueSet->containsCode("BG", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_TRUE(valueSet->containsCode("SEL", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(valueSet->containsCode("SOZ", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(valueSet->containsCode("GPV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(valueSet->containsCode("PPV", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
        EXPECT_FALSE(valueSet->containsCode("BEI", "http://fhir.de/CodeSystem/versicherungsart-de-basis"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Qualification_Type", "1.0.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(valueSet->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Qualification_Type"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_Loinc", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("9274-2", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("9271-8", "http://loinc.org"));
        EXPECT_FALSE(valueSet->containsCode("0", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("169922007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("169909004", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("0", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Value", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("77714001", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("43610007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("39910003", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("26635001", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("38107004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("24388001", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("10318004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("12431004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("64198003", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("64198003", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("169909004", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Height_Loinc", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("8302-2", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("89269-5", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Height_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("248334005", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("276351002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("276353004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("169886007", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Temp_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("386725007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("300076005", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("276885007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("708499008", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("698832009", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("698831002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("431598003", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("415974002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("415945006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("364246006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("307047009", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("860958002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("415922000", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Temp_Unit", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("Cel", "http://unitsofmeasure.org"));
        EXPECT_TRUE(valueSet->containsCode("[degF]", "http://unitsofmeasure.org"));
        using namespace std::string_literals;
        EXPECT_EQ(boost::to_lower_copy("[degF]"s), boost::to_lower_copy("[degF]"s));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_Loinc", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("29463-7", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("8339-4", "http://loinc.org"));
        EXPECT_FALSE(valueSet->containsCode("8339-5", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_Method_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("363808001", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("786458005", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("784399000", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("735395000", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("445541000", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("443245006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("425024002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("424927000", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("400967004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("364589006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("248351003", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("27113001", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("301334000", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_BodyWeight_Unit", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("g", "http://unitsofmeasure.org"));
        EXPECT_TRUE(valueSet->containsCode("kg", "http://unitsofmeasure.org"));
        EXPECT_FALSE(valueSet->containsCode("t", "http://unitsofmeasure.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Diagnose_Snomed_CT", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_FALSE(valueSet->canValidate());
        EXPECT_FALSE(valueSet->containsCode("404684003", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("272379006", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("243796009", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_BASE_GemRS_Anlage_8", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("D", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_TRUE(valueSet->containsCode("AFG", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("CY", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_FALSE(valueSet->containsCode("DE", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Head_Circumference_BodySite_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("302548004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("362865009", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Head_Circumference_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("363812007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("169876006", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Heart_Rate_Snomed", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("364075005", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("78564009", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("249043002", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_IHEXDS_AuthorSpeciality_Restricted", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "urn:oid:1.2.276.0.76.5.493"));
        EXPECT_TRUE(valueSet->containsCode("3", "urn:oid:1.2.276.0.76.5.493"));
        EXPECT_TRUE(valueSet->containsCode("010", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("544", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        EXPECT_TRUE(valueSet->containsCode("1", "urn:oid:1.2.276.0.76.5.492"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("6", "urn:oid:1.2.276.0.76.5.492"));
        EXPECT_TRUE(valueSet->containsCode("2", "urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("148", "urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Practitioner_Speciality", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "urn:oid:1.2.276.0.76.5.493"));
        EXPECT_TRUE(valueSet->containsCode("3", "urn:oid:1.2.276.0.76.5.493"));
        EXPECT_TRUE(valueSet->containsCode("010", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("544", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        EXPECT_TRUE(valueSet->containsCode("1", "urn:oid:1.2.276.0.76.5.492"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("6", "urn:oid:1.2.276.0.76.5.492"));
        EXPECT_TRUE(valueSet->containsCode("2", "urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("148", "urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11"));
        EXPECT_TRUE(valueSet->containsCode("309343006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("042", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("537", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Practitioner_Speciality_Addendum", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("309343006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("042", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        EXPECT_TRUE(valueSet->containsCode("537", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
        EXPECT_TRUE(valueSet->containsCode("143", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_BAR2_WBO"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_Base_Stage_Life", "1.1.3");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("41847000", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("263659003", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("255398004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("713153009", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("3658006", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("255407002", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("271872005", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN", "1.01");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("10", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("11", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("14", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
        EXPECT_TRUE(valueSet->containsCode("17", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PKV_TARIFF", "1.01");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PERSONENGRUPPE", "1.02");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(valueSet->containsCode("06", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(valueSet->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(valueSet->containsCode("08", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
        EXPECT_TRUE(valueSet->containsCode("09", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DMP", "1.05");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("05", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("06", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("07", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("08", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("09", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("10", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("11", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS", "1.02");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(valueSet->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(valueSet->containsCode("5", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ITA_WOP", "1.00");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("98", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_DIAGNOSESICHERHEIT", "0.9.13");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("A", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("G", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("V", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("Z", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE", "1.00");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("KA", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("KTP", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("N1", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("N2", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("N3", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("NB", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
        EXPECT_TRUE(valueSet->containsCode("Sonstiges", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_SEITENLOKALISATION", "1.00");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("B", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(valueSet->containsCode("L", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(valueSet->containsCode("R", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
    }
    {
        const auto* valueSet =
            repo().findValueSet("https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", "1.10");
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("AEO", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("AMP", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("PFT", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("ZPA", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    }
}
