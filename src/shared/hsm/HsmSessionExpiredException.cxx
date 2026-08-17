/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmSessionExpiredException.hxx"


HsmSessionExpiredException::HsmSessionExpiredException(const std::string& what, const uint32_t errorCode)
    : HsmException(what, errorCode)
{
}
