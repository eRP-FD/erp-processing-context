/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Communication.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "shared/model/Bundle.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <gtest/gtest.h>
#include <iostream>

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
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CM_Base_Terminology_Complete_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AdministrativeGender_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceCategory_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceClinicalStatusCodes_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceCriticality_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceSeverity_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceType_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_AllergyIntoleranceVerificationStatusCodes_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Apgar_Score_Identifier_LOINC_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Apgar_Score_Identifier_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Apgar_Score_Value_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Height_LOINC_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Height_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Temp_LOINC_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Temp_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Weight_LOINC_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Weight_Method_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Body_Weight_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_CommonLanguages_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_ContactPointSystem_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_DaysOfWeek_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Device_Status_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Device_Status_Reason_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Diagnosis_Clinical_Status_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Diagnosis_Severity_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Diagnosis_Verification_Status_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_DiagnosticReportStatus_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Drug_Therapy_Status_Codes_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Encounter_Class_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Encounter_Status_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Event_Status_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Event_Timing_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Glucose_Concentration_Interpretation_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Glucose_Concentration_LOINC_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Glucose_Concentration_Method_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Glucose_Concentration_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_GoalLifecycleStatus_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Head_Circumference_BodySite_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Head_Circumference_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Heart_Rate_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Marital_Status_Codes_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Medication_Status_Codes_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Medication_Type_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Medicine_Doseform_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Medicine_Route_Of_Administration_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Nursing_Professions_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_ObservationReferenceRangeMeaningCodes_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_PatientRelationshipType_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Practitioner_Speciality_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Procedure_Categories_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Procedure_Follow_Up_Codes_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Reaction_Allergy_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Respiratory_Rate_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Results_Type_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Role_Care_Team.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Rolecare.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Route_of_Administration_SNOMED_CT_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_Stage_Life_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_UnitsOfTime_German.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_CS_Base_identifier_type.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Additional_Comment.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Allergy_Intolerance_Abatement.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Goal_Medication_Target_Reference.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Medication_Type.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Responsible_Person_Organization.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Stage_Life.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_EX_Base_Treatment_Goal_End.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_NS_Base_ANR.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_NS_Base_BSNR.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_NS_Base_EBM.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Allergy_Intolerance.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_CareTeam.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Datatype_Contactpoint.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Datatype_Maiden_Name.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Datatype_Name.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Datatype_Post_Office_Box.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Datatype_Street_Address.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Device.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Device_Definition.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Diagnosis.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_DiagnosticReport.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_DocumentReference.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Encounter.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Identifier_KVK.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Identifier_Reisepassnummer.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Medication.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_MedicationStatement.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Apgar_Score.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Blood_Pressure.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Body_Height.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Body_Temperature.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Body_Weight.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Care_Level.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Glucose_Concentration.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Head_Circumference.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Heart_Rate.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Observation_Respiratory_Rate.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Organization.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Patient.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Peripheral_Oxygen_Saturation.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Practitioner.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_PractitionerRole.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Procedure.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Provenance.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_RelatedPerson.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_PR_Base_Treatment_Goal.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Allergy_Substance_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Apgar_Score_Identifier_LOINC.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Apgar_Score_Identifier_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Apgar_Score_Value.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Body_Height_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Body_Temperature_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Body_Weight_Method_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Body_Weight_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_CommonLanguages.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Deuev_Anlage_8.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Device_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Diagnosis_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Drug_Therapy_Status_Codes_SNOMED_CT_.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Event_Timing.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Glucose_Concentration_Interpretation.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Glucose_Concentration_Loinc.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Glucose_Concentration_Method_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Glucose_Concentration_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Glucose_Concentration_Unit.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Head_Circumference_BodySite_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Head_Circumference_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Heart_Rate_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Medication_Type.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Medicine_Doseform.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Medicine_Route_Of_Administration.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Nursing_Professions.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Practitioner_IHEXDS_Facharzttitel_der_Aerztekammern.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Practitioner_IHEXDS_Qualifikationen_nicht_aerztlicher_Autoren.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Practitioner_IHEXDS_Qualifikatoren_zahnaerztlicher_Autoren.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Practitioner_Speciality.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Practitioner_Speciality_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Procedure_Categories_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Procedure_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Reaction_Allergy.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Respiratory_Rate_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Results_Type.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Role_Care_Team.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Rolecare.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Route_of_Administration_SNOMED_CT.xml",
            "fhir/profiles/KBV.Basis-1.3.0/package/KBV_VS_Base_Stage_Life.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-CommonTag-De.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-KontaktDiagnoseProzedur.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-Wahlleistung-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-abdata-wg14.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-arge-ik-klassifikation.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-ask.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-behinderung-merkzeichen-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-deuev-anlage-6-vorsatzworte.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-deuev-anlage-7-namenszusaetze.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-deuev-anlage-8-laenderkennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dimdi-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dimdi-atc.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dimdi-icd-10-gm.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dimdi-ops.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-Aufnahmeanlass.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-AufnahmegrundDritteStelle.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-AufnahmegrundErsteUndZweiteStelle.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-AufnahmegrundVierteStelle.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-EntlassungsgrundDritteStelle.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-EntlassungsgrundErsteUndZweiteStelle.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-Fachabteilungsschluessel-erweitert.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-Fachabteilungsschluessel.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-dkgev-abrechnungsart.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-identifier-type-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-ifa-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-kontaktart-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-kontaktebene-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-mehrfachkodierungs-kennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-administrative-gender.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-diagnosis-role.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-identifier-type-v2.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-iso-3166.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-marital-status.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-supplement-observation-category.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/CodeSystem-versicherungsart-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Codesystem-iso-3166-2-de-laendercodes.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ConceptMap-OPS-SNOMED-Category.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Condition-related.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-AbrechnungsDiagnoseProzedur.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-Abrechnungsart.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-InformationRecipient-De.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-Lebensphase.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-destatis-ags.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-dkgev-Aufnahmegrund.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-dkgev-Entlassgrund.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-dokumentationsdatum.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-abrechnendeIK.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-besondere-personengruppe.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-dmp-kennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-einlesedatum-karte.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-generation-egk.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-kostenerstattung.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-onlinepruefung-egk.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-ruhender-leistungsanspruch.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-versichertenart.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-version-vsdm.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-wop.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-gkv-zuzahlungsstatus.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-humanname-namenszusatz.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-icd-10-gm-diagnosesicherheit.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-icd-10-gm-mehrfachcodierungs-kennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-normgroesse.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-seitenlokalisation.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Extension-wahlleistungen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-arge-ik-iknr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-asv-teamnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-bfarm-btmnr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-bundesaerztekammer-efn.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-gkv-hmnr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-gkv-kvid-10.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-gkv-kvk-versichertennummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-gkv-kvnr-30.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-gkv-pseudo-kvid.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kbv-bsnr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kbv-lanr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kbv-pnr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kbv-vknr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kzbv-zahnarztnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kzv-KzvAbrechnungsnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/NamingSystem-kzv-zahnarztnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Namingsystem-de-basis.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Observation-pflegegrad.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-address-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-chargeitem-ebm.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-alpha-id-se.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-ask.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-atc.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-icd10gm.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-ops.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coding-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coverage-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coverage-de-gkv.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-coverage-de-sel.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-humanname-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-aufnahmenummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-bsnr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-efn.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-iknr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-kvid10.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-kzva.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-lanr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-pid.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-pkv.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-pseudo-kvid.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-standortnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-telematikid.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-vknr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-identifier-zanr.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE-Koerpergewicht.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE-Koerpergroesse.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE-Koerpertemperatur.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE-Kopfumfang.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE_Atemfrequenz.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE_Blutdruck.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE_GCS.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE_Herzfrequenz.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-VitalSignDE_Periphere_Artierielle_Sauerstoffsaettigung.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-de-ekg.StructureDefinition.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-observation-grad-der-behinderung.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/Profile-reisepassnummer.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.36--20181001183306.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.38--20180713162205.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.39--20180713132816.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.40--20180713132721.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.58--20180713162142.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.59--20180713162125.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AbrechnungsDiagnoseProzedur.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AbrechnungsartVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-ArgeIkKlassifikationVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AufnahmeanlassVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AufnahmegrundDritteStelleVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AufnahmegrundErsteUndZweiteStelleVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-AufnahmegrundVierteStelleVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-CommonTagDe.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-DiagnoseTyp.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-Diagnosesubtyp.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-EkgLeads.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-EncounterClass-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-EncounterStatus-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-EntlassungsgrundDritteStelleVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-EntlassungsgrundErsteUndZweiteStelleVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-FachabteilungsschluesselErweitert.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-FachabteilungsschluesselVS.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-KontaktArt-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-KontaktEbene-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-Lebensphase.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-UcumVitalsCommonDE.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-VitalSignDE_Body_Height_Loinc.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-VitalSignDE_Body_Length_UCUM.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-VitalSignDE_Body_Weight_Loinc.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-VitalSignDE_Body_Weight_UCUM.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-Wahlleistung-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-abdata-wg14.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-alpha-id.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-ask.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-atc.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-behinderung-merkzeichen-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-dimdi-icd10-gm.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-dimdi-ops.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-gender-amtlich-de.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-icd-mehrfachkodierungs-kennzeichen.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-identifier-type-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-ifa-pzn.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-iso-3166-2-de-laendercodes.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-ops-pflegegrad.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-versicherungsart-de-basis.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/basisprofil-de-r4.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.30--20180713131246.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.31--20180713132208.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.32--20180713132315.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.33--20180713132759.xml",
            "fhir/profiles/de.basisprofil.r4-1.3.2/package/ValueSet-1.2.276.0.76.11.34--20180713132843.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/CodeSystem-GEM-ERPCHRG-CS-ConsentType.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/StructureDefinition-GEM-ERPCHRG-EX-MarkingFlag.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/StructureDefinition-GEM-ERPCHRG-PR-ChargeItem.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/StructureDefinition-GEM-ERPCHRG-PR-Communication-ChargChangeReply.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/StructureDefinition-GEM-ERPCHRG-PR-Communication-ChargChangeReq.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/StructureDefinition-GEM-ERPCHRG-PR-Consent.xml",
            "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4/package/ValueSet-GEM-ERPCHRG-VS-ConsentType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/CodeSystem-GEM-ERP-CS-AvailabilityStatus.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/CodeSystem-GEM-ERP-CS-DocumentType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/CodeSystem-GEM-ERP-CS-FlowType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/CodeSystem-GEM-ERP-CS-OrganizationType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-AbortOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-AcceptOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-ActivateOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-CloseOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-CreateOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/OperationDefinition-RejectOperation.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-AcceptDate.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-AvailabilityState.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-Beneficiary.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-ExpiryDate.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-InsuranceProvider.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-PackageQuantity.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-PrescriptionType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-SubstitutionAllowedType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-EX-SupplyOptionsType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PAR-OUT-OP-Accept.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-AccessCode.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-AuditEvent.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-BfArMApproval.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Binary.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Bundle.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-CloseOperationInputBundle.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Communication-DispReq.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Communication-InfoReq.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Communication-Reply.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Communication-Representative.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Composition.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Device.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Digest.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-MedicationDispense.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-PrescriptionId.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Secret.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Signature.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/StructureDefinition-GEM-ERP-PR-Task.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/ValueSet-GEM-ERP-VS-AvailabilityStatus.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/ValueSet-GEM-ERP-VS-DocumentType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/ValueSet-GEM-ERP-VS-FlowType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/ValueSet-GEM-ERP-VS-OrganizationType.xml",
            "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0/package/ValueSet-GEM-ERP-VS-PerformerType.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_ARZTNRFACHGRUPPE_V1.03.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_WBO_V1.16.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BMP_DOSIEREINHEIT_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DMP_V1.06.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_FORMULAR_ART_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_BAR2_ARZTNRFACHGRUPPE_V1.03.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_BMP_DOSIEREINHEIT_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ITA_WOP_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DMP_V1.06.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_FORMULAR_ART_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_NORMGROESSE_V1.00.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PKV_TARIFF_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml",
            "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM_V1.15.xml",
            "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM_V1.15.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_CS_FOR_Berufsbezeichnung.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_CS_FOR_Payor_Type_KBV.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_CS_FOR_Qualification_Type.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_CS_FOR_StatusCoPayment.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_CS_FOR_Ursache_Type.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_EX_FOR_Accident.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_EX_FOR_Alternative_IK.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_EX_FOR_Legal_basis.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_EX_FOR_PKV_Tariff.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_EX_FOR_StatusCoPayment.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_NS_FOR_Fachgruppennummer_ASV.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_NS_FOR_Pruefnummer.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_Coverage.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_Identifier_PkvID_10.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_Organization.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_Patient.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_Practitioner.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_PR_FOR_PractitionerRole.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_VS_FOR_Payor_type.xml",
            "fhir/profiles/kbv.ita.for-1.1.0/package/KBV_VS_FOR_Qualification_Type.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/CodeSystem-DAV-CS-ERP-CompositionTypes.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/CodeSystem-DAV-CS-ERP-InvoiceTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/CodeSystem-DAV-CS-ERP-MedicationDispenseTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/CodeSystem-DAV-CS-ERP-RueckspracheArzt.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/CodeSystem-DAV-CS-ERP-ZusatzattributGruppe.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Abrechnungszeilen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Gesamtzuzahlung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-KostenVersicherter.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-MwStSatz.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Rezeptaenderung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Vertragskennzeichen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Zaehler.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-Zusatzattribute.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-ZusatzdatenEinheit.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-ZusatzdatenFaktorkennzeichen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-ZusatzdatenHerstellung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Extension-DAV-EX-ERP-ZusatzdatenPreiskennzeichen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-AbgabedatenBundle.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-AbgabedatenComposition.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-Abgabeinformationen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-Abrechnungszeilen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-Apotheke.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-ZusatzdatenEinheit.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-Base-ZusatzdatenHerstellung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-ERP-AbgabedatenMeta.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-ERP-DAVHerstellerSchluessel.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/Profile-DAV-PR-ERP-PreisangabeEUR.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/ValueSet-DAV-VS-ERP-DEUEV-Anlage-8.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/ValueSet-DAV-VS-ERP-InvoiceTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/ValueSet-DAV-VS-ERP-MedicationDispenseTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1/package/ValueSet-DAV-VS-ERP-RueckspracheArzt.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/CodeSystem-DAV-PKV-CS-ERP-AbrechnungsTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/CodeSystem-DAV-PKV-CS-ERP-ArtRezeptaenderung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/CodeSystem-DAV-PKV-CS-ERP-KostenVersicherterKategorie.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/CodeSystem-DAV-PKV-CS-ERP-ZusatzattributSchluesselAutidemAustausch.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/CodeSystem-DAV-PKV-CS-ERP-ZusatzdatenEinheitFaktorkennzeichen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Extension-DAV-PKV-EX-ERP-AbrechnungsTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Extension-DAV-PKV-EX-ERP-Bankverbindung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-AbgabedatenBundle.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-AbgabedatenComposition.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-Abgabeinformationen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-Abrechnungszeilen.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-Apotheke.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-ZusatzdatenEinheit.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/Profile-DAV-PKV-PR-ERP-ZusatzdatenHerstellung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/ValueSet-DAV-PKV-VS-ERP-AbrechnungsTyp.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/ValueSet-DAV-PKV-VS-ERP-ArtRezeptaenderung.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/ValueSet-DAV-PKV-VS-ERP-KostenVersicherterKategorie.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/ValueSet-DAV-PKV-VS-ERP-ZusatzattributSchluesselAutidemAustausch.xml",
            "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0/package/ValueSet-DAV-PKV-VS-ERP-ZusatzdatenEinheitFaktorkennzeichen.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_CS_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_CS_ERP_Medication_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_CS_ERP_Section_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_BVG.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_DosageFlag.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_EmergencyServicesFee.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_CompoundingInstruction.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_Ingredient_Amount.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_Ingredient_Form.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_Packaging.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_PackagingSize.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Medication_Vaccine.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_Multiple_Prescription.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_EX_ERP_PracticeSupply_Payor.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Bundle.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Composition.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Medication_Compounding.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Medication_FreeText.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Medication_Ingredient.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Medication_PZN.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_PracticeSupply.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_PR_ERP_Prescription.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_VS_ERP_Accident_Type.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_VS_ERP_Medication_Category.xml",
            "fhir/profiles/kbv.ita.erp-1.1.2/KBV_VS_ERP_StatusCoPayment.xml",
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
    Sample{"Bundle", "test/EndpointHandlerTest/kbv_bundle_unexpected_extension.xml", {
        // the following two are caused by missing values sets - ERP-10539
        {std::make_tuple(Severity::warning, "ValueSet contains no codes after expansion"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::warning, "Cannot validate ValueSet binding"), "Bundle.entry[0].resource{Composition}.type"},
        {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "Bundle.entry[0].resource{Composition}.extension[1]"},
        {std::make_tuple(Severity::error, R"-(reference is not literal or invalid but must be resolvable: {"type": "Device", "identifier": {"system": "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer", "value": "X/000/1111/22/333"}})-"), "Bundle.entry[0].resource{Composition}.author[1]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[2].resource{Practitioner}.identifier[0].type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://fhir.de/CodeSystem/ifa/pzn, it is empty or synthesized."), "Bundle.entry[4].resource{Medication}.code.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://terminology.hl7.org/CodeSystem/v2-0203, it is empty or synthesized."), "Bundle.entry[3].resource{Organization}.identifier[0].type.coding[0]"},
        {std::make_tuple(Severity::warning, "Can not validate CodeSystem http://snomed.info/sct, it is empty or synthesized."), "Bundle.entry[4].resource{Medication}.extension[3].valueCodeableConcept.coding[0]"},
        {std::make_tuple(Severity::warning, "Unresolved CodeSystem urn:oid:1.2.276.0.76.5.514; Unresolved CodeSystem urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11; Unresolved CodeSystem urn:oid:1.2.276.0.76.5.492"), "Bundle.entry[2].resource{Practitioner}.qualification[0].code"},
        {std::make_tuple(Severity::warning, "Unresolved CodeSystem urn:oid:1.2.276.0.76.5.514; Unresolved CodeSystem urn:oid:1.3.6.1.4.1.19376.3.276.1.5.11; Unresolved CodeSystem urn:oid:1.2.276.0.76.5.492"), "Bundle.entry[2].resource{Practitioner}.qualification[1].code"},
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
    }},
    Sample{"OperationOutcome", "test/fhir-path/samples/invalid_empty_element.json", {
    }, {
        {"dom-6", "OperationOutcome"},{"ele-1", "OperationOutcome.issue[0].details"}
    }},
    Sample{"OperationOutcome", "test/fhir-path/samples/invalid_empty_array.json", {
        {std::make_tuple(Severity::error,
            "Array cannot be empty - the property should not be present if it has no values"),
            "OperationOutcome.issue[0].expression"}
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
    using namespace fhirtools::version_literal;
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Accident_Type", "1.1.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(valueSet->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_FALSE(valueSet->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
        EXPECT_TRUE(valueSet->containsCode("4", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_Medication_Category", "1.1.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Medication_Category"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_ERP_StatusCoPayment", "1.1.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("0", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
        EXPECT_TRUE(valueSet->containsCode("2", "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_StatusCoPayment"));
    }
    {
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Payor_type", "1.1.0"_ver});
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
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_FOR_Qualification_Type", "1.1.0"_ver});
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
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_LOINC", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("9274-2", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("9271-8", "http://loinc.org"));
        EXPECT_FALSE(valueSet->containsCode("0", "http://loinc.org"));
    }
    {
        const auto* valueSet = repo().findValueSet(
            {"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Identifier_SNOMED_CT", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("169922007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("169909004", "http://snomed.info/sct"));
        EXPECT_FALSE(valueSet->containsCode("0", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Apgar_Score_Value", "1.3.0"_ver});
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
            repo().findValueSet({"http://fhir.de/ValueSet/VitalSignDE_Body_Height_Loinc", "1.3.2"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("8302-2", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("89269-5", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Temperature_SNOMED_CT", "1.3.0"_ver});
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
            repo().findValueSet({"http://fhir.de/ValueSet/VitalSignDE_Body_Weight_Loinc", "1.3.2"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("29463-7", "http://loinc.org"));
        EXPECT_TRUE(valueSet->containsCode("8339-4", "http://loinc.org"));
        EXPECT_FALSE(valueSet->containsCode("8339-5", "http://loinc.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_Method_SNOMED_CT", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("363808001", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("786458005", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Body_Weight_SNOMED_CT", "1.3.0"_ver});
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
            repo().findValueSet({"http://fhir.de/ValueSet/VitalSignDE_Body_Weigth_UCUM", "1.3.2"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("g", "http://unitsofmeasure.org"));
        EXPECT_TRUE(valueSet->containsCode("kg", "http://unitsofmeasure.org"));
        EXPECT_FALSE(valueSet->containsCode("t", "http://unitsofmeasure.org"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Deuev_Anlage_8", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("D", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_TRUE(valueSet->containsCode("AFG", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("CY", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
        EXPECT_FALSE(valueSet->containsCode("DE", "http://fhir.de/CodeSystem/deuev/anlage-8-laenderkennzeichen"));
    }
    {
        const auto* valueSet = repo().findValueSet(
            {"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Head_Circumference_BodySite_SNOMED_CT", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("302548004", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("362865009", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Head_Circumference_SNOMED_CT", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("363812007", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("169876006", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Heart_Rate_SNOMED_CT", "1.3.0"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("364075005", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("78564009", "http://snomed.info/sct"));
        EXPECT_TRUE(valueSet->containsCode("249043002", "http://snomed.info/sct"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_Base_Stage_Life", "1.3.0"_ver});
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
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN", "1.01"_ver});
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
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PKV_TARIFF", "1.01"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("02", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("03", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
        EXPECT_TRUE(valueSet->containsCode("04", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PKV_TARIFF"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_PERSONENGRUPPE", "1.02"_ver});
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
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DMP", "1.06"_ver});
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
        // New in 1.106
        EXPECT_TRUE(valueSet->containsCode("12", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("30", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("57", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
        EXPECT_TRUE(valueSet->containsCode("58", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS", "1.02"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("1", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(valueSet->containsCode("3", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
        EXPECT_TRUE(valueSet->containsCode("5", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"));
    }
    {
        const auto* valueSet = repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ITA_WOP", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("00", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        EXPECT_TRUE(valueSet->containsCode("01", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("98", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_DIAGNOSESICHERHEIT", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("A", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("G", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("V", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
        EXPECT_TRUE(valueSet->containsCode("Z", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE", "1.00"_ver});
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
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_ICD_SEITENLOKALISATION", "1.00"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("B", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(valueSet->containsCode("L", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
        EXPECT_TRUE(valueSet->containsCode("R", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ICD_SEITENLOKALISATION"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", "1.15"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("AEO", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("AMP", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // ...
        EXPECT_TRUE(valueSet->containsCode("PFT", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("ZPA", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // New in 1.14:
        EXPECT_TRUE(valueSet->containsCode("IID", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("LIV", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        // New in 1.15:
        EXPECT_TRUE(valueSet->containsCode("LYE", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
        EXPECT_TRUE(valueSet->containsCode("PUE", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    }
    {
        const auto* valueSet =
            repo().findValueSet({"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_FORMULAR_ART", "1.02"_ver});
        ASSERT_TRUE(valueSet);
        EXPECT_TRUE(valueSet->canValidate());
        EXPECT_TRUE(valueSet->containsCode("e010", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(valueSet->containsCode("e011", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(valueSet->containsCode("e012", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(valueSet->containsCode("e013", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(valueSet->containsCode("e16A", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
        EXPECT_TRUE(valueSet->containsCode("e16D", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART"));
    }
}


struct FhirPathValidatorTestMiniSamplesParams {
    std::string_view name;
};

class FhirPathValidatorTestMiniSamples : public ::fhirtools::test::SampleValidationTest<FhirPathValidatorTestMiniSamples>
{

public:
    static std::list<std::filesystem::path> fileList()
    {
        return {
            "test/fhir-path/profiles/minifhirtypes.xml",
            "test/fhir-path/profiles/indirect_alternative_profiles.xml",
        };
    }

};

TEST_P(FhirPathValidatorTestMiniSamples, samples)
{
    ASSERT_NO_FATAL_FAILURE(run());
}


INSTANTIATE_TEST_SUITE_P(
    samples, FhirPathValidatorTestMiniSamples,
    ::testing::ValuesIn(std::list<fhirtools::test::Sample>{
        {"Resource", "test/fhir-path/samples/valid_IndirectAlternativeProfiles1.json"},
        {"Resource", "test/fhir-path/samples/valid_IndirectAlternativeProfiles2.json"},
        Sample{
            "Resource",
            "test/fhir-path/samples/invalid_IndirectAlternativeProfiles.json",
            {
                {{Severity::error,
                  R"(value must match fixed value: "http://fhir-tools.test/IndirectAlternativeProfile1/Extension" )"
                  R"((but is "http://fhir-tools.test/UNEXPECTED_URL"))"},
                 "Resource.contained[0]{Resource}.extension[0].url"},
                {{Severity::error,
                  R"(value must match fixed value: "http://fhir-tools.test/IndirectAlternativeProfile2/Extension" )"
                  R"((but is "http://fhir-tools.test/UNEXPECTED_URL"))"},
                 "Resource.contained[0]{Resource}.extension[0].url"},
            },
        },
    }));
