/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/ErpRequirements.hxx"

#include <optional>
#include <string>

namespace dosagetext
{

struct Language {
    static constexpr auto implements = A_28566.implements("Bereitstellen von Versionen und Sprachen");
    std::string code;
    auto operator<=>(const Language&) const = default;
    bool operator==(const Language& rhs) const = default;
    bool operator!=(const Language& rhs) const = default;
};

constexpr Language Language_DE{"de-DE"};
constexpr std::optional<Language> findLanguage(std::string_view lang)
{
    return Language_DE.code == lang ? std::make_optional(Language_DE) : std::nullopt;
}
constexpr std::string knownLanguagesList()
{
    return Language_DE.code;
}

}// dosagetext
