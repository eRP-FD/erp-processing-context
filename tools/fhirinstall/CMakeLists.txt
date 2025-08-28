#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

if (NOT TARGET fhirinstall)
    add_executable(fhirinstall EXCLUDE_FROM_ALL)
endif()

target_sources(fhirinstall
    PRIVATE
        fhirinstall_main.cxx

        FhirInstall.cxx
        FhirInstallArgs.cxx
        FhirPackage.cxx
)

target_link_libraries(fhirinstall
    PRIVATE
        erp-shared
        erp-test-resourcemanager
        fhirtools
)

add_dependencies(tools fhirinstall)
