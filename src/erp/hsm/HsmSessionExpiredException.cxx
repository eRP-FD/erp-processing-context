/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmSessionExpiredException.hxx"

#include "erp/hsm/production/HsmProductionClient.hxx"


HsmSessionExpiredException::HsmSessionExpiredException (const std::string& what, const int errorCode)
    : HsmException(what,errorCode)
{
}
