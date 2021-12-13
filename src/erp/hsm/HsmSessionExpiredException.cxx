/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmSessionExpiredException.hxx"


HsmSessionExpiredException::HsmSessionExpiredException(const std::string& what, const uint32_t errorCode)
    : HsmException(what, errorCode)
{
}
