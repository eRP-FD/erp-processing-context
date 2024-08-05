#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
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
        "schema/hl7.fhir.r4.core/4.0.1/json/fhir.json"
        "schema/shared/json/CommunicationDispReqPayload.json"
        "schema/shared/json/CommunicationReplyPayload.json"
        "schema/shared/json/draft-4-meta-schema.json"
        "schema/hl7.fhir.r4.core/4.0.1/xml/fhir.xsd"
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
          "fhir/CodeSystem-v2-0131.xml"
          "fhir/CodeSystem-v2-0203.xml"
)

set(FHIR_PROFILES_DIRS
        "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.3.1"
        "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.2.0"
        "fhir/profiles/de.basisprofil.r4-1.3.2"
        "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.0"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.3.1"
        "fhir/profiles/KBV.Basis-1.3.0"
        "fhir/profiles/kbv.ita.erp-1.1.2"
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
set(ERP_CONFIGURATION_FILE ${CMAKE_BINARY_DIR}/resources/01_production.config.json)
set(ERP_CONFIGURATION_DEVEL_FILE "")
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
set(ERP_CONFIGURATION_IN "${CMAKE_CURRENT_SOURCE_DIR}/production/01_production.config.json.in")
set(ERP_CONFIGURATION_DEVEL_IN "")
configure_file(install-config.cmake.in install-config.cmake @ONLY)
include(${CMAKE_CURRENT_BINARY_DIR}/install-config.cmake)

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
install(FILES ${CMAKE_BINARY_DIR}/resources/01_production.config.json
        DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
        COMPONENT production)
install(FILES ${CMAKE_BINARY_DIR}/resources/01_production.config.json
        DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
        COMPONENT blob-db-initialization)
if (NOT TARGET production)
    add_custom_target(production)
endif()
add_dependencies(production resources)
