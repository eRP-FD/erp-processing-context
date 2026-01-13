#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
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

set(ERP_FHIR_REPOSITORY_DIR "${CMAKE_BINARY_DIR}/fhir_package_tmp")
set(ERP_RUNTIME_RESOURCE_DIR "${CMAKE_BINARY_DIR}/bin/resources")
set(ERP_FHIR_RESOURCE_DIR "${ERP_RUNTIME_RESOURCE_DIR}/fhir/")

message(NOTICE "ERP_HL7_ORG_DEFINITIONS_FILES=${ERP_HL7_ORG_DEFINITIONS_FILES}")
add_custom_command(OUTPUT ${ERP_HL7_ORG_DEFINITIONS_FILES}
                   COMMAND "${CMAKE_COMMAND}" -E make_directory "${ERP_FHIR_RESOURCE_DIR}"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${CMAKE_COMMAND}" -E tar xJf "${CMAKE_CURRENT_SOURCE_DIR}/fhir/hl7-definitions.tar.xz"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${Patch_EXECUTABLE}" "--quiet" "-p1" "-i" "${FHIR_PATCH}"
                   COMMAND "${CMAKE_COMMAND}" -E chdir "${ERP_FHIR_RESOURCE_DIR}"
                               "${Patch_EXECUTABLE}" "--quiet" "-p1" "-i" "${FHIR_PATCH2}"
                   COMMENT "Extracting/Patching hl7-definitions.tar.xz")


include(fhir)
fhir_substitute(de.gematik.terminology@1.0.5=@1.0.5-4)

fhir_install(hl7.fhir.uv.extensions.r4 5.2.0)
fhir_install(hl7.terminology.r4 6.3.0
        PACKAGE_FIX_SCRIPT fhir/hl7.terminology.r4-6.3.0.split-legacy-codes.cmake)

fhir_install(de.basisprofil.r4 1.5.0)
fhir_install(de.basisprofil.r4 1.5.2)
fhir_install(de.basisprofil.r4 1.5.3)

fhir_install(de.gematik.epa 1.0.6-2)
fhir_install(de.gematik.epa 1.1.5)

fhir_install(de.gematik.epa.medication 1.0.6-2
        PACKAGE_FIX_SCRIPT "fhir/de.gematik.epa.medication-fix-part-type.cmake" "-DPatch_EXECUTABLE=${Patch_EXECUTABLE}"
        DEPENDS "fhir/de.gematik.epa.medication-1.0.6-2-fix-part-type.patch"
)
fhir_install(de.gematik.epa.medication 1.1.5
        PACKAGE_FIX_SCRIPT "fhir/de.gematik.epa.medication-fix-part-type.cmake" "-DPatch_EXECUTABLE=${Patch_EXECUTABLE}"
        DEPENDS "fhir/de.gematik.epa.medication-1.1.5-fix-part-type.patch"
)

fhir_install(de.gematik.fhir.directory 0.11.25)
fhir_install(de.gematik.fhir.directory 0.13.1)

fhir_install(de.gematik.terminology 1.0.5-4
        PACKAGE_FIX_SCRIPT "fhir/de.gematik.terminology-1.0.x-remove-broken-expansion.cmake"
)
fhir_install(de.gematik.terminology 1.0.6
        PACKAGE_FIX_SCRIPT "fhir/de.gematik.terminology-1.0.x-remove-broken-expansion.cmake"
)

fhir_install(de.gematik.ti 1.0.1)
fhir_install(de.gematik.ti 1.1.0)

fhir_install(dvmd.kdl.r4 2025.0.0)
fhir_install(de.ihe-d.terminology 3.0.1)

fhir_install(kbv.basis 1.7.0)
fhir_install(kbv.ita.for 1.2.1)

fhir_install(de.abda.erezeptabgabedatenbasis 1.5.0)

# Main packages:
fhir_install(de.abda.erezeptabgabedatenpkv 1.4.0)
fhir_install(de.gematik.erezept.eu 1.0.0)
fhir_install(de.gematik.erezept-patientenrechnung.r4 1.1.0)
fhir_install(de.gematik.erezept-workflow.r4 1.5.2)
fhir_install(kbv.ita.erp 1.3.3)
fhir_install(kbv.itv.evdga 1.2.2)



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

set(FHIR_PROFILES_DIRS
        "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.3.1"
        "fhir/profiles/de.abda.erezeptabgabedatenbasis-1.4.1"
        "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.2.0"
        "fhir/profiles/de.abda.erezeptabgabedatenpkv-1.3.0"
        "fhir/profiles/de.basisprofil.r4-1.3.2"
        "fhir/profiles/de.gematik.epa.medication-1.0.3/package"
        "fhir/profiles/de.gematik.erezept-patientenrechnung.r4-1.0.4"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.2.0"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.3.1"
        "fhir/profiles/de.gematik.erezept-workflow.r4-1.4.3"
        "fhir/profiles/de.gematik.fhir.directory-0.11.12/package"
        "fhir/profiles/KBV.Basis-1.3.0"
        "fhir/profiles/kbv.ita.erp-1.1.2"
        "fhir/profiles/kbv.ita.for-1.1.0/package/"
        "fhir/profiles/kbv.itv.evdga-1.1.1/package"
        "fhir/profiles/fhir.kbv.de"
        )

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

add_custom_target(resource_files ALL
    DEPENDS
        ${SCHEMA_FILES}
        ${ZDICT_FILES}
        ${ERP_EASTER_LIST}
        ${FHIR_PROFILES_DIRS}
)

if (NOT TARGET resources)
    add_custom_target(resources)
endif()

add_dependencies(resources configuration resource_files)

include(add_erp_config)
add_erp_config(001_fhir-resource-groups.config
        TEMPLATE "production/001_fhir-resource-groups.config.json.in"
        COMPONENTS production blob-db-initialization
)
add_erp_config(01_production.config
        TEMPLATE "production/01_production.config.json.in"
        COMPONENTS production blob-db-initialization
)
add_erp_config(01_production-medication-exporter.config
        TEMPLATE "production/01_production-medication-exporter.config.json.in"
        COMPONENTS production blob-db-initialization
)
add_erp_config(02_development.config
        TEMPLATE "test/02_development.config.json.in"
        NO_INSTALL
)


install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        COMPONENT production)

if (NOT TARGET production)
    add_custom_target(production)
endif()

add_dependencies(production resources)



