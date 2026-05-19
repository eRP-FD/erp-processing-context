/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/util/dosagetext/Language.hxx"
#include "shared/util/dosagetext/RenderedDosageInstruction.hxx"
#include "shared/util/dosagetext/ScriptVersion.hxx"

#include <vector>

namespace model
{
class DosageDgMP;
}
namespace dosagetext
{

struct ScriptVersion;
struct Language;

// https://ig.fhir.de/igs/medication/dosierung-textgenerierung.html
class Generator
{
public:
    static constexpr auto implements = A_28565.implements("Implementierung der Dosiertexterzeugung");
    explicit Generator(ScriptVersion version, Language language);

    RenderedDosageInstruction render(const std::vector<model::DosageDgMP>& dosages) const;

private:

    ScriptVersion mVersion;
    Language mLanguage;
};

}// dosagetext
