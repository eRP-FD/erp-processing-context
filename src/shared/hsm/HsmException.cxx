/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmException.hxx"


HsmException::HsmException(const std::string& what_, const uint32_t errorCode_)
    : std::runtime_error(what_)
    , errorCode(errorCode_)
{
}
