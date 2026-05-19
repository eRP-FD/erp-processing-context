/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/util/dosagetext/Language.hxx"
#include "shared/util/dosagetext/ScriptVersion.hxx"

#include <string>
namespace dosagetext
{

struct RenderedDosageInstruction {
    std::string renderedDosageInstruction;
    ScriptVersion scriptVersion;
    Language language;
};

}// dosagetext
