#
# (C) Copyright IBM Deutschland GmbH 2021
# (C) Copyright IBM Corp. 2021
#

set(ERP_HL7_ORG_DEFINITIONS_FILES
    fhir/hl7.org/version.info
    fhir/hl7.org/profiles-types.xml
    fhir/hl7.org/profiles-resources.xml
)

set(FHIR_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/fhir/ERP-5615-FHIR-28686-unsignedInt-positiveInt-as-integer.patch")

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
                   COMMENT "Extracting/Patching hl7-definitions.tar.xz")

set(SCHEMA_FILES
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/auditevent/Gem_erxAuditEvent.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/binary/Gem_erxBinary.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/communication/CommunicationGeneric.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/communication/Gem_erxCommunicationDispReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/communication/Gem_erxCommunicationInfoReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/communication/Gem_erxCommunicationReply.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/communication/Gem_erxCommunicationRepresentative.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/composition/Gem_erxComposition.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/composition/Gem_erxCompositionElement.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/device/Gem_erxDevice.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/AvailabilityStatusExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/BeneficiaryExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/CourierServiceExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/DateExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/InsuranceProviderExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/LanguagesExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/PackageQuantityExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/PaymentMethodExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/PrescriptionTypeExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/SubstitutionAllowedExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/extension/SupplyOptionsExtension.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/identifier/AccessCodeSecretIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/identifier/Identifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/identifier/KVNRIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/identifier/PrescriptionIDIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/identifier/TelematikIDIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/medicationdispense/Gem_erxMedicationDispense.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/organization/Gem_erxOrganization.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/organization/Gem_erxOrganizationElement.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/parameters/ActivateTaskParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/parameters/CreateTaskParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/parameters/Gem_erxAcceptOperationOutParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/receipt/Gem_erxReceiptBundle.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/signature/Gem_erxSignature.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/task/Gem_erxTask.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.0.3-1/xml/task/PerformerType.Coding.Values.xml"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/auditevent/Gem_erxAuditEvent.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/binary/Gem_erxBinary.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/CommunicationGeneric.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationDispReq.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/communication/Gem_erxCommunicationInfoReq.xsd"
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
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/identifier/TelematikIDIdentifier.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/medicationdispense/Gem_erxMedicationDispense.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/organization/Gem_erxOrganization.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/organization/Gem_erxOrganizationElement.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/parameters/ActivateTaskParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/parameters/CreateTaskParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/parameters/Gem_erxAcceptOperationOutParameters.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/receipt/Gem_erxReceiptBundle.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/signature/Gem_erxSignature.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/task/Gem_erxTask.xsd"
        "schema/de.gematik.erezept-workflow.r4/1.1.1/xml/task/PerformerType.Coding.Values.xml"
        "schema/hl7.fhir.r4.core/4.0.1/json/fhir.json"
        "schema/hl7.fhir.r4.core/4.0.1/xml/fhir.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/addresstypes/AddressType.Postbox.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/addresstypes/AddressType.Street.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/BVGExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/DMPFlagExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/DosageFlagExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/EmergencyServiceFeeExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/KennzeichenExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/LawBaseExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationCategoryExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationCompoundingInstructionExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationIngredientAmountExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationIngredientFormExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationNormSizeExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationPackagingExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/MedicationVaccineExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/NummerierungExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/PayorIdentifierExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/PayorIKExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/PayorNameExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/PayorTypeExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/PKVTariffExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/SortOfInsuranceExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/SpecialPersonGroupExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/StatusCoPaymentExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/UnfallBetriebExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/UnfallKennzeichenExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/UnfallTagExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/WOPExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/extension/ZeitraumExtension.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.BSNR.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.GKV.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.KVK.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.KZVA.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.LANR.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.PKV.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.XX.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/identifier/Identifier.ZANR.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Bundle.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Composition.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Generics.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Medication_BundleDummy.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Medication_Compounding.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Medication_FreeText.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Medication_Ingredient.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Medication_PZN.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_PracticeSupply.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_ERP_Prescription.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_Coverage.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_HumanName.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_Organization.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_Patient.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_Practitioner.xsd"
        "schema/kbv.ita.erp/1.0.1/xml/KBV_PR_FOR_PractitionerRole.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/addresstypes/AddressType.Postbox.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/addresstypes/AddressType.Street.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/BVGExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/DMPFlagExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/DosageFlagExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/EmergencyServiceFeeExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/KennzeichenExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/LawBaseExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationCategoryExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationCompoundingInstructionExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationIngredientAmountExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationIngredientFormExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationNormSizeExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationPackagingExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/MedicationVaccineExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/NummerierungExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/PayorIdentifierExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/PayorIKExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/PayorNameExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/PayorTypeExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/PKVTariffExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/SortOfInsuranceExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/SpecialPersonGroupExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/StatusCoPaymentExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/UnfallBetriebExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/UnfallKennzeichenExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/UnfallTagExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/WOPExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/extension/ZeitraumExtension.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.BSNR.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.GKV.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.KVK.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.KZVA.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.LANR.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.PKV.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.XX.xsd"
        "schema/kbv.ita.erp/1.0.2/xml/identifier/Identifier.ZANR.xsd"
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
        "schema/shared/json/BaseSchema.json"
        "schema/shared/json/draft-4-meta-schema.json"
        "schema/shared/json/IKNR.json"
        "schema/shared/json/XX-Type.json"
        "schema/shared/xml/GenericTypes.xsd"
        "schema/shared/xml/fhir-xhtml.xsd"
        "schema/shared/xml/xml.xsd"
        "schema/tsl/BNA_tsl.xsd"
        "schema/tsl/Gematik_tsl.xsd"
        "schema/tsl/xml.xsd"
        "schema/tsl/xmldsig-core-schema.xsd"
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
