/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/common/HttpMethod.hxx"


std::string toString (HttpMethod method)
{
    switch (method)
    {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";

        default:
        case HttpMethod::UNKNOWN: return "UNKNOWN";
    }
}


std::ostream& operator<< (std::ostream& out, HttpMethod method)
{
    out << toString(method);
    return out;
}
