This directory contains configuration and templates for generation of root CA, sub CA ( TSL signer )
and signing the TSL templates using the script scripts/generate_pki_test_data.sh.

The generated results are placed in resources/test/generated_pki folder, that has following structure.
For each created CA there's a subdirectory, which contains, besides some management data for the CA,
all certificates and associated private keys issued by the CA.
resources/test/generated_pki layout:

# <ca>/                         - a directory for each created CA
#   ca.pem                      - the CA certificate (PEM format)
#   ca.der                      - the CA certificate (DER format)
#   ca_cert_chain.pem           - the CA and its issuer chain up to the root (concatenated, PEM format)
#   certificates/               - a directory for certificates issued by the CA
#     <entityName>/             - a directory for each entity for which a certificate has been issued
#       <entityName>.pem        - certificate for the entity (PEM format)
#       <entityName>_key.pem    - private key for the entity (unencrypted, PEM format)
#   private/                    - a directory for the private CA data
#     ca_key.pem                - the CA's private key (PEM format)
# signatures/                   - a directory for signatures created by this script
#   <signature>.txt             - a signature (base 64)
# tsl/                          - a directory for generated TSLs
#   TSL<siffix>.xml                     - TSL, signed by tsl_signer_ec (issuer sub_ca1_ec)
