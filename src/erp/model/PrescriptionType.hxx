/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_PRESCRIPTIONTYPE_HXX
#define ERP_PROCESSING_CONTEXT_PRESCRIPTIONTYPE_HXX

#include "erp/pc/ProfessionOid.hxx"

#include <magic_enum.hpp>
#include <cstdint>
#include <unordered_map>

namespace model
{

// https://simplifier.net/erezept-workflow/flowtype
enum class PrescriptionType : uint8_t
{
    apothekenpflichigeArzneimittel = 160u,
    direkteZuweisung = 169u
};

// mappings defined in A_19445
static const std::unordered_map<PrescriptionType, std::string_view> PrescriptionTypeDisplay{
    {PrescriptionType::apothekenpflichigeArzneimittel, "Muster 16 (Apothekenpflichtige Arzneimittel)"},
    {PrescriptionType::direkteZuweisung, "Muster 16 (Direkte Zuweisung)"}};

static const std::unordered_map<PrescriptionType, std::string> PrescriptionTypePerformerType{
    {PrescriptionType::apothekenpflichigeArzneimittel,
     "urn:oid:" + std::string(profession_oid::oid_oeffentliche_apotheke)},
    {PrescriptionType::direkteZuweisung, "urn:oid:" + std::string(profession_oid::oid_oeffentliche_apotheke)}};

static const std::unordered_map<PrescriptionType, std::string_view> PrescriptionTypePerformerDisplay{
    {PrescriptionType::apothekenpflichigeArzneimittel, "Öffentliche Apotheke"},
    {PrescriptionType::direkteZuweisung, "Öffentliche Apotheke"}};
}

// the default supported enum range needs to be customized. The default range is [-128,128]
namespace magic_enum::customize
{
template<>
struct enum_range<model::PrescriptionType> {
    static constexpr int min = 160;// Must be greater than `INT16_MIN`.
    static constexpr int max = 169;// Must be less than `INT16_MAX`.
};
}// namespace magic_enum

// used mainly by google test output
namespace model
{
inline std::ostream& operator<<(std::ostream& os, const model::PrescriptionType& prescriptionType)
{
    return os << model::PrescriptionTypeDisplay.at(prescriptionType);
}
}

#endif//ERP_PROCESSING_CONTEXT_PRESCRIPTIONTYPE_HXX
