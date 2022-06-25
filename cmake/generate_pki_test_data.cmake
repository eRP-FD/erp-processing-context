#
# (C) Copyright IBM Deutschland GmbH 2022
# (C) Copyright IBM Corp. 2022
#

# Enforce use of conan-provided OpenSSL
find_program(OPENSSL
    NAMES openssl
    PATHS "${CONAN_BIN_DIRS_OPENSSL}"
    REQUIRED)
mark_as_advanced(OPENSSL)
set(ENV{OPENSSL} "${OPENSSL}")
set(ENV{LD_LIBRARY_PATH} "${CONAN_LIB_DIRS_OPENSSL}:$ENV{LD_LIBRARY_PATH}")

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
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_pki_test_data.sh --clean --output-dir=${CMAKE_BINARY_DIR}
        OUTPUT_QUIET
        ERROR_QUIET
        COMMAND_ECHO STDOUT
)

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
