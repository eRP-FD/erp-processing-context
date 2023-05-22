/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/HsmException.hxx"


HsmException::HsmException(const std::string& what_, const uint32_t errorCode_)
    : std::runtime_error(what_)
    , errorCode(errorCode_)
{
}
