/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/common/HttpStatus.hxx"
#include "erp/util/Expect.hxx"

#include <string>

HttpStatus fromBoostBeastStatus (uint32_t status)
{
    switch(status)
    {
#define STATUS(boostBeastValue, value, code) case code: return HttpStatus::value;
#include "HttpStatus.inc.hxx"
#include "HttpStatusNonStandard.inc.hxx"
#undef STATUS
        default: Fail2("unsupported status code " + std::to_string(status), std::runtime_error);
    }
}


const char* toString (const HttpStatus status)
{
#define STATUS(boostBeastValue, value, code) case HttpStatus::value: return #value;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
#include "HttpStatusNonStandard.inc.hxx"
        default: Fail2("unsupported HttpStatus value", std::logic_error);
    }
#undef STATUS
}


size_t toNumericalValue (const HttpStatus status)
{
#define STATUS(boostBeastValue, value, code) case HttpStatus::value: return code;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
#include "HttpStatusNonStandard.inc.hxx"
        default: Fail2("unsupported HttpStatus value", std::logic_error);
    }
#undef STATUS

}


std::ostream& operator<< (std::ostream& stream, const HttpStatus& status)
{
    stream << toString(status);
    return stream;
}
