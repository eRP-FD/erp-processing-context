#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

# Enforce use of conan-provided OpenSSL
unset(OPENSSL CACHE)

find_program(OPENSSL
    NAMES openssl
    PATHS
        ${openssl_BIN_DIRS_RELEASE}
        ${openssl_BIN_DIRS_RELWITHDEBINFO}
        ${openssl_BIN_DIRS_MINSIZEREL}
        ${openssl_BIN_DIRS_DEBUG}
    NO_DEFAULT_PATH
    REQUIRED)
mark_as_advanced(OPENSSL)
set(OPENSSL_LD_LIBARAY_PATH
    ${openssl_OpenSSL_Crypto_LIB_DIRS_DEBUG}
    ${openssl_OpenSSL_Crypto_LIB_DIRS_RELWITHDEBINFO}
    ${openssl_OpenSSL_Crypto_LIB_DIRS_MINSIZEREL}
    ${openssl_OpenSSL_Crypto_LIB_DIRS_RELEASE}
)

# Check availability of executables used in script
find_program(DD
    NAMES dd
    REQUIRED)
mark_as_advanced(DD)
find_program(XXD
    NAMES xxd
    REQUIRED)
mark_as_advanced(XXD)
find_program(XMLSEC1
    NAMES xmlsec1
    REQUIRED)
mark_as_advanced(XMLSEC1)

execute_process(
    COMMAND
        ${CMAKE_COMMAND} -E env "OPENSSL=${OPENSSL}" "LD_LIBRARY_PATH=${OPENSSL_LD_LIBARAY_PATH}:$ENV{LD_LIBRARY_PATH}"
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_pki_test_data.sh --clean --output-dir=${CMAKE_BINARY_DIR}
    COMMAND_ECHO STDOUT
    OUTPUT_VARIABLE OUTPUT_GENERATE_PKI_TEST_DATA
    ERROR_VARIABLE ERROR_GENERATE_PKI_TEST_DATA
    RESULT_VARIABLE RESULT_GENERATE_PKI_TEST_DATA
)

if(RESULT_GENERATE_PKI_TEST_DATA AND NOT RESULT_GENERATE_PKI_TEST_DATA EQUAL 0)
    message(FATAL_ERROR "Failed to generate PKI test data\n${OUTPUT_GENERATE_PKI_TEST_DATA}\n${ERROR_GENERATE_PKI_TEST_DATA}")
endif()

file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/BNA_valid.xml
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/generated_pki/tsl)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/QES-noTypeCA.base64.der
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/QES-noType.base64.der
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/DefaultOcsp.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/DefaultOcsp.prv.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/IDP-Wansim.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/IDP-Wansim-CA.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/qes.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/GEM.HBA-qCA6_TEST-ONLY.der
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/nonQesSmcb.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/nonQesSmcbPrivateKey.pem
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/nonQesSmcbIssuer.der
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)

file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/idpResponseJwk.txt
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
file(COPY ${CMAKE_SOURCE_DIR}/resources/test/tsl/X509Certificate/idpResponse.json
        DESTINATION ${CMAKE_BINARY_DIR}/resources/test/tsl/X509Certificate/)
