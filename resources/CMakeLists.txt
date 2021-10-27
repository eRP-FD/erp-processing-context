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
        "schema/json/AvailabilityStateExtension.json"
        "schema/json/BaseSchema.json"
        "schema/json/draft-4-meta-schema.json"
        "schema/json/fhir.json"
        "schema/json/Gem_baseTelematikID.json"
        "schema/json/Gem_erxAcceptOperationOutParameters.json"
        "schema/json/Gem_erxAuditEvent.json"
        "schema/json/Gem_erxBinary.json"
        "schema/json/Gem_erxCommunicationDispReq.json"
        "schema/json/Gem_erxCommunicationInfoReq.json"
        "schema/json/Gem_erxCommunicationReply.json"
        "schema/json/Gem_erxCommunicationRepresentative.json"
        "schema/json/Gem_erxCompositionElement.json"
        "schema/json/Gem_erxDevice.json"
        "schema/json/Gem_erxLocation.json"
        "schema/json/Gem_erxMedicationDispense.json"
        "schema/json/Gem_erxPrescriptionID.json"
        "schema/json/Gem_erxReceiptBundle.json"
        "schema/json/Gem_erxSignature.json"
        "schema/json/Gem_erxTask.json"
        "schema/json/IKNR.json"
        "schema/json/InsuranceProvider.json"
        "schema/json/kbv/ExtensionGkvBesonderePersonengruppe.json"
        "schema/json/kbv/ExtensionGkvDmpKennzeichen.json"
        "schema/json/kbv/KBV_EX_ERP_Accident.json"
        "schema/json/kbv/KBV_EX_ERP_BVG.json"
        "schema/json/kbv/KBV_EX_ERP_DosageFlag.json"
        "schema/json/kbv/KBV_EX_ERP_EmergencyServicesFee.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_Category.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_CompoundingInstruction.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_Ingredient_Amount.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_Ingredient_Form.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_normgroesse.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_Packaging.json"
        "schema/json/kbv/KBV_EX_ERP_Medication_Vaccine.json"
        "schema/json/kbv/KBV_EX_ERP_Multiple_Prescription.json"
        "schema/json/kbv/KBV_EX_ERP_PracticeSupply_Payor.json"
        "schema/json/kbv/KBV_EX_ERP_StatusCoPayment.json"
        "schema/json/kbv/KBV_EX_FOR_Legal_basis.json"
        "schema/json/kbv/KBV_EX_FOR_PKV_Tariff.json"
        "schema/json/kbv/KBV_PR_ERP_Bundle.json"
        "schema/json/kbv/KBV_PR_ERP_Composition.json"
        "schema/json/kbv/KBV_PR_ERP_Medication_Compounding.json"
        "schema/json/kbv/KBV_PR_ERP_Medication_FreeText.json"
        "schema/json/kbv/KBV_PR_ERP_Medication_Ingredient.json"
        "schema/json/kbv/KBV_PR_ERP_Medication_PZN.json"
        "schema/json/kbv/KBV_PR_ERP_PracticeSupply.json"
        "schema/json/kbv/KBV_PR_ERP_Prescription.json"
        "schema/json/kbv/KBV_PR_FOR_Coverage.json"
        "schema/json/kbv/KBV_PR_FOR_Organization.json"
        "schema/json/kbv/KBV_PR_FOR_Patient.json"
        "schema/json/kbv/KBV_PR_FOR_Practitioner.json"
        "schema/json/kbv/KBV_PR_FOR_PractitionerRole.json"
        "schema/json/kbv/KennzeichenWohnortprinzip.json"
        "schema/json/kbv/VersichertenartGKV.json"
        "schema/json/KVID.json"
        "schema/json/PackageQuantity.json"
        "schema/json/PrescriptionType.json"
        "schema/json/SubstitutionAllowed.json"
        "schema/json/SupplyOptionsType.json"
        "schema/json/XX-Type.json"
        "schema/xml/auditevent/Gem_erxAuditEvent.xsd"
        "schema/xml/binary/Gem_erxBinary.xsd"
        "schema/xml/bundle/Gem_erxBundle.xsd"
        "schema/xml/communication/CommunicationGeneric.xsd"
        "schema/xml/communication/Gem_erxCommunicationDispReq.xsd"
        "schema/xml/communication/Gem_erxCommunicationInfoReq.xsd"
        "schema/xml/communication/Gem_erxCommunicationReply.xsd"
        "schema/xml/communication/Gem_erxCommunicationRepresentative.xsd"
        "schema/xml/composition/Gem_erxComposition.xsd"
        "schema/xml/composition/Gem_erxCompositionElement.xsd"
        "schema/xml/device/Gem_erxDevice.xsd"
        "schema/xml/extension/AvailabilityStatusExtension.xsd"
        "schema/xml/extension/BeneficiaryExtension.xsd"
        "schema/xml/extension/CourierServiceExtension.xsd"
        "schema/xml/extension/DateExtension.xsd"
        "schema/xml/extension/InsuranceProviderExtension.xsd"
        "schema/xml/extension/LanguagesExtension.xsd"
        "schema/xml/extension/PackageQuantityExtension.xsd"
        "schema/xml/extension/PaymentMethodExtension.xsd"
        "schema/xml/extension/PrescriptionTypeExtension.xsd"
        "schema/xml/extension/SubstitutionAllowedExtension.xsd"
        "schema/xml/extension/SupplyOptionsExtension.xsd"
        "schema/xml/GenericTypes.xsd"
        "schema/xml/identifier/AccessCodeSecretIdentifier.xsd"
        "schema/xml/identifier/Identifier.xsd"
        "schema/xml/identifier/PrescriptionIDIdentifier.xsd"
        "schema/xml/kbv/addresstypes/AddressType.Postbox.xsd"
        "schema/xml/kbv/addresstypes/AddressType.Street.xsd"
        "schema/xml/kbv/extension/BVGExtension.xsd"
        "schema/xml/kbv/extension/DMPFlagExtension.xsd"
        "schema/xml/kbv/extension/DosageFlagExtension.xsd"
        "schema/xml/kbv/extension/EmergencyServiceFeeExtension.xsd"
        "schema/xml/kbv/extension/KennzeichenExtension.xsd"
        "schema/xml/kbv/extension/LawBaseExtension.xsd"
        "schema/xml/kbv/extension/MedicationCategoryExtension.xsd"
        "schema/xml/kbv/extension/MedicationCompoundingInstructionExtension.xsd"
        "schema/xml/kbv/extension/MedicationIngredientAmountExtension.xsd"
        "schema/xml/kbv/extension/MedicationIngredientFormExtension.xsd"
        "schema/xml/kbv/extension/MedicationNormSizeExtension.xsd"
        "schema/xml/kbv/extension/MedicationPackagingExtension.xsd"
        "schema/xml/kbv/extension/MedicationVaccineExtension.xsd"
        "schema/xml/kbv/extension/NummerierungExtension.xsd"
        "schema/xml/kbv/extension/PayorIdentifierExtension.xsd"
        "schema/xml/kbv/extension/PayorIKExtension.xsd"
        "schema/xml/kbv/extension/PayorNameExtension.xsd"
        "schema/xml/kbv/extension/PayorTypeExtension.xsd"
        "schema/xml/kbv/extension/PKVTariffExtension.xsd"
        "schema/xml/kbv/extension/SortOfInsuranceExtension.xsd"
        "schema/xml/kbv/extension/SpecialPersonGroupExtension.xsd"
        "schema/xml/kbv/extension/StatusCoPaymentExtension.xsd"
        "schema/xml/kbv/extension/UnfallBetriebExtension.xsd"
        "schema/xml/kbv/extension/UnfallKennzeichenExtension.xsd"
        "schema/xml/kbv/extension/UnfallTagExtension.xsd"
        "schema/xml/kbv/extension/WOPExtension.xsd"
        "schema/xml/kbv/extension/ZeitraumExtension.xsd"
        "schema/xml/kbv/identifier/Identifier.BSNR.xsd"
        "schema/xml/kbv/identifier/Identifier.GKV.xsd"
        "schema/xml/kbv/identifier/Identifier.KVK.xsd"
        "schema/xml/kbv/identifier/Identifier.LANR.xsd"
        "schema/xml/kbv/identifier/Identifier.PKV.xsd"
        "schema/xml/kbv/identifier/Identifier.XX.xsd"
        "schema/xml/kbv/identifier/Identifier.ZANR.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Bundle.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Composition.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Generics.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Medication_BundleDummy.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Medication_Compounding.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Medication_FreeText.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Medication_Ingredient.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Medication_PZN.xsd"
        "schema/xml/kbv/KBV_PR_ERP_PracticeSupply.xsd"
        "schema/xml/kbv/KBV_PR_ERP_Prescription.xsd"
        "schema/xml/kbv/KBV_PR_FOR_Coverage.xsd"
        "schema/xml/kbv/KBV_PR_FOR_HumanName.xsd"
        "schema/xml/kbv/KBV_PR_FOR_Organization.xsd"
        "schema/xml/kbv/KBV_PR_FOR_Patient.xsd"
        "schema/xml/kbv/KBV_PR_FOR_Practitioner.xsd"
        "schema/xml/kbv/KBV_PR_FOR_PractitionerRole.xsd"
        "schema/xml/medicationdispense/Gem_erxMedicationDispense.xsd"
        "schema/xml/organization/Gem_erxOrganization.xsd"
        "schema/xml/organization/Gem_erxOrganizationElement.xsd"
        "schema/xml/parameters/ActivateTaskParameters.xsd"
        "schema/xml/parameters/CreateTaskParameters.xsd"
        "schema/xml/parameters/Gem_erxAcceptOperationOutParameters.xsd"
        "schema/xml/receipt/Gem_erxReceiptBundle.xsd"
        "schema/xml/signature/Gem_erxSignature.xsd"
        "schema/xml/task/Gem_erxTask.xsd"
        "schema/xml/tsl/BNA_tsl.xsd"
        "schema/xml/tsl/Gematik_tsl.xsd"
        "schema/xml/tsl/xmldsig-core-schema.xsd"
        "schema/xml/tsl/xml.xsd"
        "schema/xml/xhtml.xsd"
        "schema/xml/fhir.xsd"
        "schema/xml/fhir-xhtml.xsd"
        "schema/xml/xml.xsd"
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
set(ERP_CONFIGURATION_FILE ${CMAKE_INSTALL_FULL_BINDIR}/configuration.json)
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
configure_file(install-config.cmake.in install-config.cmake @ONLY)



# create install script for build tree
set(ERP_CONFIGURATION_FILE ${CMAKE_BINARY_DIR}/bin/configuration.json)
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_BINARY_DIR}/bin/resources")
configure_file(install-config.cmake.in install-config-devel.cmake @ONLY)

add_custom_command(
    OUTPUT ${ERP_CONFIGURATION_FILE}
    DEPENDS test/configuration.json.in
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/install-config-devel.cmake
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/install-config-devel.cmake
)

add_custom_target(resources ALL
    DEPENDS
        ${ERP_HL7_ORG_DEFINITIONS_FILES}
        ${SCHEMA_FILES}
        ${ERP_CONFIGURATION_FILE}
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
