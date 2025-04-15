/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/EnumTable.hxx"
#include "library/util/Assert.hxx"

#include "shared/util/String.hxx"


std::vector<std::string> splitRawEnumNames(std::string_view rawNames)
{
    std::vector<std::string> result = String::split(rawNames, ',');
    for (std::string& name : result)
    {
        name = String::trim(name);
        Assert(name.find_first_of(" =") == std::string::npos) << "initializers are not supported";
    }
    return result;
}
