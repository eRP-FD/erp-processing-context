/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Extension.hxx"

namespace model
{

class GeneratedDosageInstructionsMeta : public Extension<GeneratedDosageInstructionsMeta>
{
public:
    using Extension::Extension;
    static constexpr auto url = resource::structure_definition::GeneratedDosageInstructionsMeta;

    [[nodiscard]] std::string_view algorithmVersion() const;
    [[nodiscard]] std::string_view language() const;

    template<typename T>
    friend class Parameters;
};

extern template class Extension<GeneratedDosageInstructionsMeta>;
extern template class Resource<GeneratedDosageInstructionsMeta>;

}// model
