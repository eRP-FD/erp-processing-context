/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/HsmSessionExpiredException.hxx"


HsmSessionExpiredException::HsmSessionExpiredException(const std::string& what, const uint32_t errorCode)
    : HsmException(what, errorCode)
{
}
