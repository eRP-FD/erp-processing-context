/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_COMMON_HTTPSTATUS_HXX
#define E_LIBRARY_COMMON_HTTPSTATUS_HXX

#include <ostream>

enum class HttpStatus
{
#define STATUS(boostBeastValue, value, code) value = code,
#include "HttpStatus.inc.hxx"
#undef STATUS
};

HttpStatus fromBoostBeastStatus (uint32_t status);
const char* toString (HttpStatus status);
size_t toNumericalValue (HttpStatus status);
std::ostream& operator <<(std::ostream& stream, const HttpStatus& status);

#endif
