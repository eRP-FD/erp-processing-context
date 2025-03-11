#
# (C) Copyright IBM Deutschland GmbH 2021, 2024
# (C) Copyright IBM Corp. 2021, 2024
#
# non-exclusively licensed to gematik GmbH
#

# Values used in *config.json.in
set(ERP_SCHEMA_DIR "${ERP_RUNTIME_RESOURCE_DIR}/schema/")
set(ERP_FHIR_RESOURCE_DIR "${ERP_RUNTIME_RESOURCE_DIR}/fhir/")
set(ERP_ZSTD_DICTIONARY_DIR "${ERP_RUNTIME_RESOURCE_DIR}/zstd/")

configure_file(${ERP_CONFIGURATION_FILE_IN} ${ERP_CONFIGURATION_FILE} @ONLY)
