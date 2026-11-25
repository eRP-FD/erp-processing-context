/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Expect.hxx"

#include "shared/util/ErpException.hxx"

#include <openssl/err.h>


std::string local::sslErrors()
{
    std::string errors;
    while (unsigned long error = ERR_get_error())
    {
        if (! errors.empty())
        {
            errors += "; ";
        }

        errors += ERR_error_string(error, nullptr);
    }
    return errors;
}
