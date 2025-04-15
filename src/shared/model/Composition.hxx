/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMPOSITION_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMPOSITION_HXX

#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

// https://applications.kbv.de/S_KBV_STATUSKENNZEICHEN_V1.01.xhtml
enum class KbvStatusKennzeichen
{
    ohneErsatzverordnungskennzeichen = 0,
    asvKennzeichen = 1,
    entlassmanagementKennzeichen = 4,
    tssKennzeichen = 7,
    nurErsatzverordnungsKennzeichen = 10,
    asvKennzeichenMitErsatzverordnungskennzeichen = 11,
    entlassmanagementKennzeichenMitErsatzverordungskennzeichen = 14,
    tssKennzeichenMitErsatzverordungskennzeichen = 17
};

// Reduced version of Composition resource, contains only functionality currently needed;
// NOLINTNEXTLINE(bugprone-exception-escape)
class Composition : public Resource<Composition>
{
public:
    static constexpr auto resourceTypeName = "Composition";
    static constexpr auto profileType = ProfileType::Gem_erxCompositionElement;

    Composition(const std::string_view& telematicId, const model::Timestamp& start, const model::Timestamp& end,
                const std::string_view& author, const std::string_view& prescriptionDigestIdentifier);

    [[nodiscard]] std::string_view id() const;
    [[nodiscard]] std::optional<std::string_view> telematikId() const;
    [[nodiscard]] std::optional<model::Timestamp> date() const;
    [[nodiscard]] std::optional<model::Timestamp> periodStart() const;
    [[nodiscard]] std::optional<model::Timestamp> periodEnd() const;
    [[nodiscard]] std::optional<std::string_view> author() const;
    [[nodiscard]] std::optional<std::string_view> prescriptionDigestIdentifier() const;
    // idx may be 0 or 1
    [[nodiscard]] std::optional<std::string_view> authorIdentifierSystem(size_t idx) const;


    // accessor for KBV_EX_FOR_Legal_basis in extension array of KBV_PR_ERP_Composition
    [[nodiscard]] std::optional<KbvStatusKennzeichen> legalBasisCode() const;

private:
    friend Resource<Composition>;
    explicit Composition(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Composition>;
}


#endif
