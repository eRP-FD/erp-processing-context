#
# (C) Copyright IBM Deutschland GmbH 2021, 2023
# (C) Copyright IBM Corp. 2021, 2023
#
# non-exclusively licensed to gematik GmbH
#

set(ERP_HL7_ORG_DEFINITIONS_FILES
    fhir/hl7.org/version.info
    fhir/hl7.org/profiles-types.xml
    fhir/hl7.org/profiles-resources.xml
    fhir/hl7.org/extension-definitions.xml
    fhir/hl7.org/valuesets.xml
    fhir/hl7.org/v3-codesystems.xml
)

set(FHIR_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/fhir/ERP-5615-FHIR-28686-unsignedInt-positiveInt-as-integer.patch")
set(FHIR_PATCH2 "${CMAKE_CURRENT_SOURCE_DIR}/fhir/v3-codesystems-version-2018-08-12-to-4.0.1.patch")

set(ZDICT_FILES
    zstd/default_json.zdict
    zstd/default_xml.zdict
)

find_package(Patch REQUIRED)

list(TRANSFORM ERP_HL7_ORG_DEFINITIONS_FILES PREPEND "${CMAKE_BINARY_DIR}/bin/resources/")

set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_BINARY_DIR}/bin/resources")
set(ERP_FHIR_RESOURCE_DIR "${ERP_RUNTIME_RESOURCE_DIR}/fhir/")
message(NOTICE "ERP_HL7_ORG_DEFINITIONS_FILES=${ERP_HL7_ORG_DEFINITIONS_FILES}")
add_custom_command(OUTPUT ${ERP_HL7_ORG_DEFINITIONS_FILES}
                   COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_FHIR_RESOURCE_DIR}"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${CMAKE_COMMAND}" -E tar xJf "${CMAKE_CURRENT_SOURCE_DIR}/fhir/hl7-definitions.tar.xz"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${Patch_EXECUTABLE}" "-p1" "-i" "${FHIR_PATCH}"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${Patch_EXECUTABLE}" "-p1" "-i" "${FHIR_PATCH2}"
                   COMMENT "Extracting/Patching hl7-definitions.tar.xz")

set(SCHEMA_FILES
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/auditevent/Gem_erxAuditEvent.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/binary/Gem_erxBinary.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/bundle/MedicationDispenseBundle.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/CommunicationGeneric.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationDispReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationInfoReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationChargChangeReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationChargChangeReply.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationReply.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationRepresentative.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/composition/Gem_erxComposition.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/composition/Gem_erxCompositionElement.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/device/Gem_erxDevice.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/AvailabilityStatusExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/BeneficiaryExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/CourierServiceExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/DateExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/InsuranceProviderExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/LanguagesExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/PackageQuantityExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/PaymentMethodExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/PrescriptionTypeExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/SubstitutionAllowedExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/extension/SupplyOptionsExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/AccessCodeSecretIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/Identifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/KVNRIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/PrescriptionIDIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/PrescriptionIdAccessCodeIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/TelematikIDIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/medicationdispense/Gem_erxMedicationDispense.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/organization/Gem_erxOrganization.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/organization/Gem_erxOrganizationElement.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/parameters/Gem_erxAcceptOperationOutParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/receipt/Gem_erxReceiptBundle.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/signature/Gem_erxSignature.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/task/Gem_erxTask.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/task/PerformerType.Coding.Values.xml"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/chargeitem/Gem_erxChargeItem.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/consent/Gem_erxConsent.xsd"
        "schema/hl7.fhir.r4.core/4.0.1/json/fhir.json"
        "schema/hl7.fhir.r4.core/4.0.1/xml/fhir.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/addresstypes/AddressType.Street.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Bundle.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Composition.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Generics.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Medication_BundleDummy.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Medication_Compounding.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Medication_FreeText.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Medication_Ingredient.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Medication_PZN.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_PracticeSupply.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_ERP_Prescription.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_Coverage.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_HumanName.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_Organization.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_Patient.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_Practitioner.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/KBV_PR_FOR_PractitionerRole.xsd"
        "schema/not-profiled/parameters/ActivateTaskParameters.xsd"
        "schema/not-profiled/parameters/CreateTaskParameters.xsd"
        "schema/shared/json/BaseSchema.json"
        "schema/shared/json/draft-4-meta-schema.json"
        "schema/shared/json/IKNR.json"
        "schema/shared/json/XX-Type.json"
        "schema/shared/xml/constrained-strings.xsd"
        "schema/shared/xml/GenericTypes.xsd"
        "schema/shared/xml/fhir-xhtml.xsd"
        "schema/shared/xml/xml.xsd"
        "schema/tsl/BNA_tsl.xsd"
        "schema/tsl/Gematik_tsl.xsd"
        "schema/tsl/xml.xsd"
        "schema/tsl/xmldsig-core-schema.xsd"
        "schema/vsdm/Pruefungsnachweis.xsd"
        )

foreach(schema ${SCHEMA_FILES})
    add_custom_command (
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_RUNTIME_RESOURCE_DIR}"
            OUTPUT "${ERP_RUNTIME_RESOURCE_DIR}/${schema}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/production/${schema}"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/production/${schema}" "${ERP_RUNTIME_RESOURCE_DIR}/${schema}"
            COMMENT "copy ${CMAKE_CURRENT_SOURCE_DIR}/production/${schema} ${ERP_RUNTIME_RESOURCE_DIR}/${schema}"
    )
endforeach()
set(FHIR_PROFILES
          "fhir/CodeSystem-flowType-supplements.xml"
          "fhir/CodeSystem-v2-0131.xml"
          "fhir/CodeSystem-v2-0203.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/abortOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/acceptOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/activateOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/AvailabilityStateExtension.StructureDefinition.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/BeneficiaryExtension.StructureDefinition.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/closeOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-availabilityStatus.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-consenttype.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-documentType.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CodeSystem-flowType.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/CourierService.StructureDefinition.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/createOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/erxDevice.StructureDefinition.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/NamingSystem-accesscode.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/NamingSystem-PRESCRIPTIONID.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/NamingSystem-secret.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/NamingSystem-telematikID.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/rejectOperation.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-AuditEvent-ErxAuditEvent.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Binary.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Bundle-GemErxBundle.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-ChargeItem-erxChargeItem.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationDispReq.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationInfoReq.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Communication-erxCommunicationRepresentative.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Composition-GemErxComposition2.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Consent-erxConsent.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-AcceptDate.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-ExpiryDate.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-InsuranceProvider.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-MarkingFlag.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-PackageQuanity.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-PrescriptionType.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-SubstitutionAllowedType.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Extension-SupplyOptionsType.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Identifier-PRESCRIPTIONID.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Identifier-TELEMATIKID.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-MedicationDispense-ERxMedicationDispense.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Organization-ERxOrganization.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Parameters-erxAcceptOperationOutParameters.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Signature-ErxSignature.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/StructureDefinition-Task-ERxTask.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-AVAILABILITYSTATUS.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-CONSENTTYPE.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-DOCUMENTTYPE.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-FLOWTYPE.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-ORGANIZATIONTYPE.xml"
          "fhir/profiles/de.gematik.erezept-workflow.r4-1.1.1/ValueSet-PERFORMERTYPE.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Payor_Type_KBV.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Qualification_Type.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_CS_FOR_Ursache_Type.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_Alternative_IK.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_Legal_basis.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_EX_FOR_PKV_Tariff.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_NS_FOR_Pruefnummer.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Coverage.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Organization.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Patient.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_Practitioner.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_PR_FOR_PractitionerRole.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_VS_FOR_Payor_type.xml"
          "fhir/profiles/kbv.ita.for-1.0.3/KBV_VS_FOR_Qualification_Type.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/basisprofil-de-r4.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-arge-ik-klassifikation.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-ask.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-aufnahmeart.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-6-vorsatzworte.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-7-namenszusaetze.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-deuev-anlage-8-laenderkennzeichen.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-alpha-id.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-alpha-id-se.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-atc.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-icd-10-gm.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-dimdi-ops.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-edqm-doseform.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-gender-amtlich-de.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-identifier-type-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-ifa-pzn.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Codesystem-iso-3166-2-de-laendercodes.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-administrative-gender.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-diagnosis-role.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-identifier-type-v2.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-iso-3166.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-loinc-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-marital-status.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-supplement-observation-category.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/CodeSystem-versicherungsart-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-destatis-ags.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gender-amtlich-de.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-besondere-personengruppe.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-dmp-kennzeichen.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-einlesedatum-karte.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-generation-egk.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-kostenerstattung.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-onlinepruefung-egk.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-ruhender-leistungsanspruch.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-versichertenart.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-version-vsdm.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-wop.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-gkv-zuzahlungsstatus.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-humanname-namenszusatz.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-ausrufezeichen.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-diagnosesicherheit.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-manifestationscode.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-icd-10-gm-primaercode.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-normgroesse.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Extension-seitenlokalisation.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-arge-ik-iknr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-asv-teamnummer.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-bfarm-btmnr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-bundesaerztekammer-efn.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Namingsystem-de-basis.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-gkv-hmnr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-gkv-kvid-10.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-gkv-kvk-versichertennummer.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-gkv-kvnr-30.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-gkv-pseudo-kvid.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kbv-bsnr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kbv-lanr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kbv-pnr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kbv-vknr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kzbv-zahnarztnummer.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kzv-KzvAbrechnungsnummer.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/NamingSystem-kzv-zahnarztnummer.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Observation-pflegegrad.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-address-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-chargeitem-ebm.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-alpha-id.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-alpha-id-se.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-ask.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-atc.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-icd10gm.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-ops.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coding-pzn.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-condition-de-icd10.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-gkv.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-coverage-de-sel.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-humanname-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-bsnr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-efn.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-iknr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-kvid10.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-kzva.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-lanr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pid.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pkv.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-pseudo-kvid.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-vknr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-identifier-zanr.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Atemfrequenz.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Blutdruck.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Koerpergewicht.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Koerpergroesse.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Koerpertemperatur.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE-Kopfumfang.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Puls.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE_Sauerstoffsaettigung.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/Profile-observation-VitalSignDE.StructureDefinition.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/UcumVitalsCommonDE.ValueSet.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.30--20180713131246.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.31--20180713132208.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.32--20180713132315.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.33--20180713132759.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.34--20180713132843.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.35--20181214170712.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.36--20181001183306.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.37--20190517134631.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.38--20180713162205.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.39--20180713132816.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.40--20180713132721.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.58--20180713162142.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-1.2.276.0.76.11.59--20180713162125.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-alpha-id.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ask.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-atc.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-Aufnahmeart.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-dimdi-icd10-gm.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-gender-amtlich-de.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-identifier-type-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ifa-pzn.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-iso-3166-2-de-laendercodes.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-kbv-s-icd-diagnosesicherheit.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-ops-pflegegrad.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-versicherungsart-de-basis.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-VitalSignDE-Koerpergewicht-Snomed.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/ValueSet-VitalSignDE-Koerpergroesse-Snomed.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Height_Loinc.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Length_UCUM.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Weight_Loinc.xml"
          "fhir/profiles/de.basisprofil.r4-0.9.13/VitalSignDE_Body_Weight_UCUM.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseApgarScoreIdentifierGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseApgarScoreValueGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseBodyHeightSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CM_Base_Body_Temperature_Snomed_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CM_Base_Body_Temp_Unit_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseBodyWeightLoincGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseBodyWeightMethodSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseBodyWeightSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseDiagnosisSeverityGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseHeadCircumferenceBodySiteSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseHeadCircumferenceSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CM_Base_Heart_Rate_Snomed_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBasePractitionerFunctionGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCMBaseStageLifeGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseApgarScoreIdentifierGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseApgarScoreValueGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyHeightSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Body_Temperature_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Body_Temp_Unit_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightLoincGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightMethodSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseBodyWeightSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseDiagnosisSeverityGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseHeadCircumferenceBodySiteSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseHeadCircumferenceSnomedGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_CS_Base_Heart_Rate_German.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseidentifiertype.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBasePractitionerFunctionGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVCSBaseStageLifeGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseAdditionalComment.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseStageLife.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVEXBaseTerminologyGerman.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVNSBaseANR.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVNSBaseBSNR.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVNSBaseEBM.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseDiagnosis.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Heart_Rate.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationApgarScore.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Blood_Pressure_New.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationBodyHeight.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Body_Temperature.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationBodyWeight.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseObservationHeadCircumference.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Observation_Respiratory_Rate.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBaseOrganization.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBasePatient.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_PR_Base_Peripheral_Oxygen_Saturation.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVPRBasePractitioner.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreIdentifierLoinc.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreIdentifierSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseApgarScoreValue.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyHeightLoinc.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyHeightSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Body_Temperature_Snomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Body_Temp_Unit.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightLoinc.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightMethodSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseBodyWeightUnit.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseDiagnoseSnomedCT.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBASEGemRSAnlage8.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseHeadCircumferenceBodySiteSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseHeadCircumferenceSnomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBV_VS_Base_Heart_Rate_Snomed.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseIHEXDSAuthorSpecialityRestricted.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBasePractitionerSpecialityAddendum.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBasePractitionerSpeciality.xml"
          "fhir/profiles/kbv.basis-1.1.3/KBVVSBaseStageLife.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Medication_Category.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Medication_Type.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_Section_Type.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_CS_ERP_StatusCoPayment.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Accident.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_BVG.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_DosageFlag.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_EmergencyServicesFee.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Category.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_CompoundingInstruction.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Ingredient_Amount.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Ingredient_Form.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Packaging.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Medication_Vaccine.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_Multiple_Prescription.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_PracticeSupply_Payor.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_EX_ERP_StatusCoPayment.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Bundle.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Composition.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_Compounding.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_FreeText.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_Ingredient.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Medication_PZN.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_PracticeSupply.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_PR_ERP_Prescription.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_Accident_Type.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_Medication_Category.xml"
          "fhir/profiles/kbv.ita.erp-1.0.2/KBV_VS_ERP_StatusCoPayment.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_BAR2_WBO_V1.16.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_DIAGNOSESICHERHEIT_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_ITA_WOP_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM_V1.12.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_DMP_V1.05.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_FORMULAR_ART_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_NORMGROESSE_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_PKV_TARIFF_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ICD_SEITENLOKALISATION_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_ITA_WOP_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM_V1.12.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_DMP_V1.05.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_FORMULAR_ART_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_NORMGROESSE_V1.00.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PERSONENGRUPPE_V1.02.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_PKV_TARIFF_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_STATUSKENNZEICHEN_V1.01.xml"
          "fhir/profiles/fhir.kbv.de/KBV_VS_SFHIR_KBV_VERSICHERTENSTATUS_V1.02.xml"
)

set(FHIR_PROFILES_DIRS
        "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.3.1"
        "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.2.0"
        "fhir/profiles/de.basisprofil.r4-1.3.2"
        "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.0"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0"
        "fhir/profiles/KBV.Basis-1.3.0"
        "fhir/profiles/kbv.ita.erp-1.1.1"
        "fhir/profiles/kbv.ita.for-1.1.0/package/"
        "fhir/profiles/fhir.kbv.de"
        )

foreach(profile ${FHIR_PROFILES})
    add_custom_command (
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_RUNTIME_RESOURCE_DIR}"
            OUTPUT "${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${profile}"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${profile}" "${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
            COMMENT "copy ${CMAKE_CURRENT_SOURCE_DIR}/${profile} ${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
    )
endforeach()
foreach(profile ${FHIR_PROFILES_DIRS})
    add_custom_command (
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_RUNTIME_RESOURCE_DIR}"
            OUTPUT "${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${profile}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${profile}" "${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
            COMMENT "copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${profile} ${ERP_RUNTIME_RESOURCE_DIR}/${profile}"
    )
endforeach()

list(TRANSFORM SCHEMA_FILES PREPEND "${ERP_RUNTIME_RESOURCE_DIR}/")

set(ERP_SCHEMA_DIR "${ERP_RUNTIME_RESOURCE_DIR}/schema/")


message(STATUS "ZDICT_FILES=${ZDICT_FILES}")
foreach(dictionary ${ZDICT_FILES})
    add_custom_command (
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_RUNTIME_RESOURCE_DIR}"
            OUTPUT "${ERP_RUNTIME_RESOURCE_DIR}/${dictionary}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${dictionary}"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${dictionary}" "${ERP_RUNTIME_RESOURCE_DIR}/${dictionary}"
            COMMENT "copy ${CMAKE_CURRENT_SOURCE_DIR}/${dictionary} ${ERP_RUNTIME_RESOURCE_DIR}/${dictionary}"
    )
endforeach()

list(TRANSFORM ZDICT_FILES PREPEND "${ERP_RUNTIME_RESOURCE_DIR}/")

set(ERP_EASTER_LIST easter.csv)
add_custom_command (
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_RUNTIME_RESOURCE_DIR}"
        OUTPUT "${ERP_RUNTIME_RESOURCE_DIR}/${ERP_EASTER_LIST}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/production/${ERP_EASTER_LIST}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/production/${ERP_EASTER_LIST}" "${ERP_RUNTIME_RESOURCE_DIR}/${ERP_EASTER_LIST}"
        COMMENT "copy ${CMAKE_CURRENT_SOURCE_DIR}/production/${ERP_EASTER_LIST} ${ERP_RUNTIME_RESOURCE_DIR}/${ERP_EASTER_LIST}"
)
set(ERP_EASTER_LIST ${ERP_RUNTIME_RESOURCE_DIR}/easter.csv)
message(STATUS "ERP_EASTER_LIST=${ERP_EASTER_LIST}")

# create install script for installation
set(ERP_CONFIGURATION_FILE ${CMAKE_INSTALL_FULL_BINDIR}/01_production.config.json)
set(ERP_CONFIGURATION_DEVEL_FILE "")
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
set(ERP_CONFIGURATION_IN "${CMAKE_CURRENT_SOURCE_DIR}/production/01_production.config.json.in")
set(ERP_CONFIGURATION_DEVEL_IN "")
configure_file(install-config.cmake.in install-config.cmake @ONLY)



# create install script for build tree
set(ERP_CONFIGURATION_FILE ${CMAKE_BINARY_DIR}/bin/01_production.config.json)
set(ERP_CONFIGURATION_DEVEL_FILE ${CMAKE_BINARY_DIR}/bin/02_development.config.json)
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_BINARY_DIR}/bin/resources")
set(ERP_CONFIGURATION_IN "${CMAKE_CURRENT_SOURCE_DIR}/production/01_production.config.json.in")
set(ERP_CONFIGURATION_DEVEL_IN "${CMAKE_CURRENT_SOURCE_DIR}/test/02_development.config.json.in")
configure_file(install-config.cmake.in install-config-devel.cmake @ONLY)

add_custom_command(
    OUTPUT ${ERP_CONFIGURATION_FILE}
    OUTPUT ${ERP_CONFIGURATION_DEVEL_FILE}
    DEPENDS production/01_production.config.json.in
    DEPENDS test/02_development.config.json.in
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/install-config-devel.cmake
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/install-config-devel.cmake
)

add_custom_target(resources ALL
    DEPENDS
        ${ERP_HL7_ORG_DEFINITIONS_FILES}
        ${SCHEMA_FILES}
        ${ERP_CONFIGURATION_FILE}
        ${ERP_CONFIGURATION_DEVEL_FILE}
        ${ZDICT_FILES}
        ${ERP_EASTER_LIST}
        ${FHIR_PROFILES}
        ${FHIR_PROFILES_DIRS}
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        COMPONENT production)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install-config.cmake
        COMPONENT production)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install-config.cmake
        COMPONENT blob-db-initialization)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production resources)


set(SAMPLEDATA_ARCHIVES
        "test/Konnektathon/7. Konnektathon/#7_Konnektathon_mycare.zip"
        "test/Konnektathon/7. Konnektathon/ADV_Konnektathon_7.zip"
        "test/Konnektathon/7. Konnektathon/Data-AL_Konnektathon7.zip"
        "test/Konnektathon/7. Konnektathon/Data_Vital_Konnektathon-02-11-21.zip"
        "test/Konnektathon/7. Konnektathon/InterData_Konnektathon.zip"
        "test/Konnektathon/7. Konnektathon/Konnektathon-7-Shop-Apotheke.zip"
        "test/Konnektathon/7. Konnektathon/M1_Konnektathon-02-11-21.zip"
        "test/Konnektathon/7. Konnektathon/M1_Konnektathon-03-11-21.zip"
        "test/Konnektathon/7. Konnektathon/MEDISTAR_Konnektathon-02-03-11-21.zip"
        "test/Konnektathon/7. Konnektathon/MedVision_Konnektathon7.zip"
        "test/Konnektathon/7. Konnektathon/Medatixx2_Konnektathon-02-03-11.zip"
        "test/Konnektathon/7. Konnektathon/Pharmatechnik_IXOS.7z"
        "test/Konnektathon/7. Konnektathon/REDMedical.7z"
        "test/Konnektathon/7. Konnektathon/REDMedicalPharma.zip"
        "test/Konnektathon/7. Konnektathon/ifa_systems_ag-03-11-21.zip"
        "test/Konnektathon/8. Konnektathon/8 Konnektathon - medisoftware - Raum1.zip"
        "test/Konnektathon/8. Konnektathon/Apotechnik.7z"
        "test/Konnektathon/8. Konnektathon/Konnektathon8.ADV.zip"
        "test/Konnektathon/8. Konnektathon/Pharmatechnik_IXOS_8_Konnektathon.7z"
        "test/Konnektathon/8. Konnektathon/konnektathon8_pharmatechnik_XT.7z"
        "test/Kostbarkeiten_der_Abgabe_Abrechnung/Kostbarkeiten_der_Abgabe_Abrechnung.zip"
        "test/MVO_KBV_1.0.2_.zip"
        )
find_program(BASH_EXECUTABLE NAMES bash REQUIRED)

message(NOTICE "Extracting sample archives ${SAMPLEDATA_ARCHIVES}")
foreach (archive ${SAMPLEDATA_ARCHIVES})
    get_filename_component(extension ${archive} LAST_EXT)
    string(REPLACE ${extension} "" base_name ${archive})
    file(ARCHIVE_EXTRACT
            INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${archive}
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${base_name})
    execute_process(
        COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/convert_kbv_bundles_1.0.1-2.sh" "${CMAKE_CURRENT_BINARY_DIR}/${base_name}")
endforeach ()
