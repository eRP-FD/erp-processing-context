/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/common/HttpStatus.hxx"

#include <string>

HttpStatus fromBoostBeastStatus (uint32_t status)
{
    switch(status)
    {
#define STATUS(boostBeastValue, value, code) case code: return HttpStatus::value;
#include "HttpStatus.inc.hxx"
#undef STATUS
        default: throw std::runtime_error("unsupported status code " + std::to_string(status));
    }
}


const char* toString (const HttpStatus status)
{
#define STATUS(boostBeastValue, value, code) case HttpStatus::value: return #value;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
        default: throw std::logic_error("unsupported HttpStatus value");
    }
#undef STATUS
}


size_t toNumericalValue (const HttpStatus status)
{
#define STATUS(boostBeastValue, value, code) case HttpStatus::value: return code;
    switch(status)
    {
#include "HttpStatus.inc.hxx"
        default: throw std::logic_error("unsupported HttpStatus value");
    }
#undef STATUS

}


std::ostream& operator<< (std::ostream& stream, const HttpStatus& status)
{
    stream << toString(status);
    return stream;
}
