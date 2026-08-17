#
# (C) Copyright IBM Deutschland GmbH 2021, 2026
# (C) Copyright IBM Corp. 2021, 2026
#
# non-exclusively licensed to gematik GmbH
#

install(
    FILES
        enrolment-helper.sh
        auto-enrolment.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
    COMPONENT blob-db-initialization
)
