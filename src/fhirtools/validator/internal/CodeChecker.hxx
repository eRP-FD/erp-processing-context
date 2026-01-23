// (C) Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
//
// non-exclusively licensed to gematik GmbH


#pragma once


#include <string_view>

namespace fhirtools
{

class ValidationResults;

class CodeChecker
{
public:
    static ValidationResults checkCode(std::string_view code, std::string_view elementFullPath);
};

}
