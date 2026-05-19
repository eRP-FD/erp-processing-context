/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/ErpRequirements.hxx"

#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>

namespace dosagetext
{

enum class ScriptVersionId
{
    V_1_0_1
};
struct ScriptVersion {
    static constexpr auto implements = A_28566.implements("Bereitstellen von Versionen und Sprachen");
    static constexpr auto implements2 =
        A_28573.implements("Versionen und Sprachen des dgMP-DosageTextgenerierung-Skript");
    ScriptVersionId versionId;
    std::string version;
    auto operator<=>(const ScriptVersion&) const = default;
};

constexpr ScriptVersion VERSION_1_0_1{.versionId = ScriptVersionId::V_1_0_1, .version = "1.0.1"};
constexpr std::optional<ScriptVersion> findVersion(const std::string_view version)
{
    return VERSION_1_0_1.version == version ? std::make_optional(VERSION_1_0_1) : std::nullopt;
}
std::string knownVersionsList();

}// dosagetext
