/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "GeneratedDosageInstructionsMeta.hxx"

namespace model
{

std::string_view GeneratedDosageInstructionsMeta::algorithmVersion() const
{
    auto ext = getExtension("algorithmVersion");
    ModelExpect(ext, "Extension algorithmVersion not found");
    auto ver = ext->valueString();
    ModelExpect(ver, "Extension algorithmVersion has no value");
    return *ver;
}

std::string_view GeneratedDosageInstructionsMeta::language() const
{
    auto ext = getExtension("language");
    ModelExpect(ext, "Extension language not found");
    auto lang = ext->valueCode();
    ModelExpect(lang, "Extension language has no value");
    return *lang;
}

template class Extension<GeneratedDosageInstructionsMeta>;
}
