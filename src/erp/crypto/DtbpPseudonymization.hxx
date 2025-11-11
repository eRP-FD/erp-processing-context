/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <string>
#include "shared/util/SafeString.hxx"

class DtbpPseudonymization
{
public:

    /**
     * Encrypt the data according to A_27332-01 and
     * return the base64 string
     **/
    static std::string encryptLogData(std::string_view data, const SafeString& key);

    /**
     * Decrypt the data according to A_27332-01 and
     * return the clear text data
     **/
    static std::string decryptLogData(std::string_view base64Data, const SafeString& key);
};
