/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "shared/ErpRequirements.hxx"

#include <vector>

namespace model
{
class GeneratedDosageInstructionsMeta;
class UnspecifiedExtension;
class DosageDgMP;
}
namespace dosagetext
{

class Validator
{
public:
    static void validate(const std::vector<model::DosageDgMP>& dosages,
                         const model::UnspecifiedExtension& renderedDosageExtension,
                         const model::GeneratedDosageInstructionsMeta& meta);
};

}// dosagetext
