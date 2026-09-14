#
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH
#

add_executable(cfgreport cfgreport.cxx)
target_link_libraries(cfgreport
    PUBLIC
        erp-shared
)

