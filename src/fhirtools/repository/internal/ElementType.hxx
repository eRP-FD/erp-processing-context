/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once


#include <set>
#include <string>

namespace fhirtools::internal
{

struct ElementType {
    std::string code;
    std::set<std::string> profiles;
    std::set<std::string> targetProfiles;
};

}
