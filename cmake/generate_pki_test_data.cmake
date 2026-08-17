#
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH
#

# Enforce use of conan-provided OpenSSL
unset(OPENSSL CACHE)
unset(XMLSEC1 CACHE)

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
set(OPENSSL_LD_LIBRARY_PATH
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
        ${CMAKE_COMMAND} -E env "OPENSSL=${OPENSSL}" "OPENSSL_CONF=/dev/null" "XMLSEC1=${XMLSEC1}" "LD_LIBRARY_PATH=${OPENSSL_LD_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_pki_test_data.sh --clean --output-dir=${CMAKE_BINARY_DIR}
    COMMAND_ECHO STDOUT
    OUTPUT_VARIABLE OUTPUT_GENERATE_PKI_TEST_DATA
    ERROR_VARIABLE ERROR_GENERATE_PKI_TEST_DATA
    RESULT_VARIABLE RESULT_GENERATE_PKI_TEST_DATA
)

if(RESULT_GENERATE_PKI_TEST_DATA AND NOT RESULT_GENERATE_PKI_TEST_DATA EQUAL 0)
    message(FATAL_ERROR "Failed to generate PKI test data\n${OUTPUT_GENERATE_PKI_TEST_DATA}\n${ERROR_GENERATE_PKI_TEST_DATA}")
endif()

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


#############################################################################
## Generrate push notification PKI
## see: https://github.ibmgcloud.net/eRp/pushgateway-mock/blob/main/tls/README.md
set(push_pki_input_folder "${CMAKE_SOURCE_DIR}/resources/test/generated_pki_input/push")

## resources/test/generated_pki is deleted by `generate_pki_test_data.sh` when running cmake, which would fully rebuild the PKI
# any running docker container would need restart - so we are not using that folder
set(push_pki_output_folder "${CMAKE_BINARY_DIR}/resources/test/generated_pki_push")

set(openssl_cnf "${CMAKE_SOURCE_DIR}/resources/test/generated_pki_input/openssl.cnf")
set(run_openssl "${CMAKE_COMMAND}" -E env
        LD_LIBRARY_PATH="${OPENSSL_LD_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}"
        OPENSSL_CONF="${openssl_cnf}"
        CRL_DISTRIBUTION_POINTS="URI:http://example.com/example_ca.crl"
        POPP_OID="OID:1.2.276.0.76.4.320"
        GENERATE_PKI_INPUT="${CMAKE_SOURCE_DIR}/resources/test/generated_pki_input"
            "${OPENSSL}"
)
function(add_fullchain_target basedir name)
    set(leafCertFile "${basedir}/${name}/${name}-cert.pem")
    set(certChain "${leafCertFile}")
    get_property(issuer SOURCE "${leafCertFile}" PROPERTY ISSUER_CERT)
    while(issuer)
        list(APPEND certChain "${issuer}")
        get_property(issuer SOURCE "${issuer}" PROPERTY ISSUER_CERT)
    endwhile()
    add_custom_command(
        OUTPUT "${basedir}/${name}/${name}-fullchain.pem"
        DEPENDS ${certChain}
        COMMAND "${CMAKE_COMMAND}" -E cat ${certChain} > "${basedir}/${name}/${name}-fullchain.pem"
    )
endfunction()

function(new_certificate name)
    cmake_parse_arguments(arg "ROOT;CA" "ISSUER;SUBJECT;CONFIG" "" ${ARGN})
    set(fulldir "${push_pki_output_folder}/${name}")
    if (arg_ROOT AND DEFINED arg_ISSUER)
        message(FATAL_ERROR "new_certificate: ISSUER ${arg_ISSUER} and ROOT ${arg_ROOT} are mutually exclusive")
    endif()
    if (NOT DEFINED arg_ROOT AND NOT DEFINED arg_ISSUER)
        message(FATAL_ERROR "new_certificate: provide either ROOT or ISSUER")
    endif()
    if (NOT DEFINED arg_SUBJECT)
        message(FATAL_ERROR "new_certificate: missing SUBJECT")
    endif()
    set(config "${openssl_cnf}")
    if (arg_CONFIG)
        set(config "${push_pki_input_folder}/${arg_CONFIG}")
    endif()
    # cleanup and rebuild CA
    add_custom_command(
        OUTPUT "${fulldir}/${name}-key.pem"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${fulldir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${fulldir}"
        COMMAND ${run_openssl} ecparam -name prime256v1 -genkey -noout -out "${fulldir}/${name}-key.pem"
        WORKING_DIRECTORY "${fulldir}"
    )
    unset(setup_ca)
    if (arg_ROOT OR arg_CA)
        set(setup_ca
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${fulldir}/certsdb"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${fulldir}/certsdb"
            COMMAND "${CMAKE_COMMAND}" -E echo "01" > "${fulldir}/crlnumber"
            COMMAND "${CMAKE_COMMAND}" -E echo_append > "${fulldir}/index.txt"
        )
    endif()
    if (arg_ROOT)
        add_custom_command(
            OUTPUT "${fulldir}/${name}-cert.pem"
            DEPENDS
                "${fulldir}/${name}-key.pem"
                "${req_config}"
            ${setup_ca}
            COMMAND ${run_openssl} req -x509 -new -days 3650 -sha256
                        -key "${fulldir}/${name}-key.pem"
                        -out "${fulldir}/${name}-cert.pem"
                        -subj "${arg_SUBJECT}"
                        -config "${config}"
            WORKING_DIRECTORY "${fulldir}"
        )
    else()
        add_custom_command(
            OUTPUT "${fulldir}/${name}-req.pem"
            DEPENDS
                "${fulldir}/${name}-key.pem"
                "${config}"
            COMMAND ${run_openssl} req -new
                        -key "${fulldir}/${name}-key.pem"
                        -out "${fulldir}/${name}-req.pem"
                        -subj "${arg_SUBJECT}"
                        -config "${config}"
            WORKING_DIRECTORY "${fulldir}"
        )
        set(issuerdir "${push_pki_output_folder}/${arg_ISSUER}")
        unset(extensions)
        add_custom_command(
            OUTPUT "${fulldir}/${name}-cert.pem"
            DEPENDS
                "${config}"
                "${issuerdir}/${arg_ISSUER}-key.pem"
                "${issuerdir}/${arg_ISSUER}-cert.pem"
                "${fulldir}/${name}-req.pem"
            ${setup_ca}
            COMMAND grep -Ev "${arg_SUBJECT}" "${issuerdir}/index.txt" > "${issuerdir}/index.txt.tmp" || true
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different  "${issuerdir}/index.txt.tmp" "${issuerdir}/index.txt"
            COMMAND ${run_openssl} ca -days 1825 -md sha256 -create_serial
                        -in "${fulldir}/${name}-req.pem"
                        -cert "${issuerdir}/${arg_ISSUER}-cert.pem"
                        -keyfile "${issuerdir}/${arg_ISSUER}-key.pem"
                        -out "${fulldir}/${name}-cert.pem"
                        -config "${config}"
                        -batch
            WORKING_DIRECTORY "${issuerdir}"
            JOB_POOL "${arg_ISSUER}_issue_cert"
        )
        set_property(SOURCE "${fulldir}/${name}-cert.pem" PROPERTY ISSUER_CERT "${issuerdir}/${arg_ISSUER}-cert.pem")
        add_custom_target(${name}-certificate DEPENDS "${fulldir}/${name}-cert.pem")
        add_dependencies(${arg_ISSUER}-signed-certificates ${name}-certificate)
    endif()
    add_fullchain_target("${push_pki_output_folder}" "${name}")
    set(${name}_FULLCHAIN "${fulldir}/${name}-fullchain.pem" PARENT_SCOPE)
    set(${name}_CERT "${fulldir}/${name}-cert.pem" PARENT_SCOPE)
    set(${name}_KEY "${fulldir}/${name}-key.pem" PARENT_SCOPE)
    if (arg_CA OR arg_ROOT)
        ## don't issue certs in parallel to avoid conflicts on index.txt
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS "${name}_issue_cert=1")
        add_custom_target(${name}-signed-certificates)
        set(${name}_CRL "${fulldir}/${name}-crl.pem" PARENT_SCOPE)
        add_custom_command(
            OUTPUT
                "${fulldir}/${name}-crl.pem"
                "${fulldir}/${name}.crl"
            DEPENDS
                "${fulldir}/${name}-key.pem"
                "${fulldir}/${name}-cert.pem"
            COMMAND ${run_openssl} ca -gencrl -crldays 365
                        -keyfile "${fulldir}/${name}-key.pem"
                        -cert "${fulldir}/${name}-cert.pem"
                        -out "${fulldir}/${name}-crl.pem"
            COMMAND ${run_openssl} crl -outform der
                        -in "${fulldir}/${name}-crl.pem"
                        -out "${fulldir}/${name}.crl"
            WORKING_DIRECTORY "${fulldir}"
        )
    endif()
endfunction()

### generate root certificate
new_certificate(rootCA ROOT
    SUBJECT "/C=DE/ST=Hamburg/L=Hamburg/O=IBM/OU=RootCA/CN=TEST-Internet-Root-CA"
)
new_certificate(intermediateCA CA
    SUBJECT "/C=DE/ST=Hamburg/L=Hamburg/O=IBM/OU=RootCA/CN=TEST-Internet-Intermediate-CA"
    ISSUER rootCA
    CONFIG intermediateCA_ext.cnf
)
new_certificate(server
    SUBJECT "/C=DE/ST=Hamburg/L=Hamburg/O=TEST-PushGateway/CN=TEST-PushGateway Mock"
    ISSUER intermediateCA
    CONFIG server_ext.cnf
)
new_certificate(client
    SUBJECT "/C=DE/ST=Hamburg/L=Hamburg/O=TestClient/OU=Testing/CN=test-client"
    ISSUER rootCA
    CONFIG policy_anything.cnf
)

# truststore with root CA
find_program(KEYTOOL names keytool REQUIRED)

add_custom_command(
    OUTPUT "${push_pki_output_folder}/truststore.jks"
    DEPENDS "${rootCA_CERT}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${push_pki_output_folder}/truststore.jks"
    COMMAND "${KEYTOOL}" -importcert -noprompt -alias internetca -trustcacerts -storetype jks -storepass changeit
        -file "${rootCA_CERT}"
        -keystore "${push_pki_output_folder}/truststore.jks"
    WORKING_DIRECTORY "${push_pki_output_folder}"
)

# Create PKCS#12 keystore containing the private key and full chain
add_custom_command(
    OUTPUT "${push_pki_output_folder}/keystore.p12"
    DEPENDS "${server_FULLCHAIN}" "${server_KEY}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${push_pki_output_folder}/keystore.p12"
    COMMAND ${run_openssl} pkcs12 -export -password pass:changeit -name pushgatewayserver
            -in "${server_FULLCHAIN}"
            -inkey "${server_KEY}"
            -out "${push_pki_output_folder}/keystore.p12"
    COMMAND chmod 666 "${push_pki_output_folder}/keystore.p12"
    WORKING_DIRECTORY "${push_pki_output_folder}"
)

add_custom_command(
    OUTPUT "${push_pki_output_folder}/keystore.jks"
    DEPENDS "${push_pki_output_folder}/keystore.p12"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${push_pki_output_folder}/keystore.jks"
    COMMAND "${KEYTOOL}" -importkeystore -deststorepass changeit -destkeypass changeit
                -srcstoretype PKCS12 -alias pushgatewayserver -srcstorepass changeit
                -destkeystore "${push_pki_output_folder}/keystore.jks"
                -srckeystore "${push_pki_output_folder}/keystore.p12"
    WORKING_DIRECTORY "${push_pki_output_folder}"
)

###################
# force building required files:
add_custom_target(push-pki ALL
    DEPENDS
        "${push_pki_output_folder}/truststore.jks"
        "${push_pki_output_folder}/keystore.p12"
        "${push_pki_output_folder}/keystore.jks"
        "${server_FULLCHAIN}"
        "${client_FULLCHAIN}"
        "${intermediateCA_CRL}"
)
