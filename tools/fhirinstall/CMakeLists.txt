#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

add_executable(fhirinstall EXCLUDE_FROM_ALL
    fhirinstall_main.cxx

    FhirInstall.cxx
    FhirPackage.cxx
)
target_link_libraries(fhirinstall
    PRIVATE
        erp-shared
        erp-test-resourcemanager
        fhirtools
)

add_dependencies(tools fhirinstall)
