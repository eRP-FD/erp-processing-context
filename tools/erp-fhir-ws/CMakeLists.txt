#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

add_executable(erp-fhir-ws EXCLUDE_FROM_ALL
    erp-fhir-ws-main.cpp

    ErpFhirWS.cxx
    WsValidateHandler.cxx
    WSViewService.cxx
)
install_libs_for(erp-fhir-ws erp-fhir-ws)
target_link_libraries(erp-fhir-ws
    PUBLIC
        erp-processing-context-files
        erp-test-util-lib
        erp-test-lib
        fhirtools
)
target_compile_definitions(erp-fhir-ws PRIVATE BOOST_ASIO_NO_DEPRECATED)
install(TARGETS erp-fhir-ws DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT erp-fhir-ws)

