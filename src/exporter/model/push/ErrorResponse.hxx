/*
* (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include <string>

namespace model
{

class ErrorResponse
{
public:
    static ErrorResponse parseResponse(const std::string& responseBody);

    [[nodiscard]] const std::string& getError() const;

private:
    ErrorResponse(std::string error);
    std::string mError;
};

}// model
