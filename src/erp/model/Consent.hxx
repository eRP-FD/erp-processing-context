/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX

#include "shared/model/Kvnr.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class Consent : public Resource<Consent>
{
public:
    static constexpr auto resourceTypeName = "Consent";
    static constexpr ProfileType profileType = ProfileType::GEM_ERPCHRG_PR_Consent;
    enum class Type
    {
        CHARGCONS
    };
    static constexpr auto chargingConsentType = magic_enum::enum_name(Type::CHARGCONS);

    static std::string createIdString(Consent::Type type, const model::Kvnr& kvnr);
    static std::pair<Consent::Type, std::string> splitIdString(const std::string_view& idStr);

    // The resource id field is filled by the constructor:
    Consent(
        const model::Kvnr& kvnr,
        const model::Timestamp& dateTime);

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] Kvnr patientKvnr() const;
    [[nodiscard]] bool isChargingConsent() const;
    [[nodiscard]] model::Timestamp dateTime() const;

    void fillId();

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend Resource<Consent>;
    explicit Consent(NumberAsStringParserDocument&& jsonTree);

    void setId(const std::string_view& id);
    void setPatientKvnr(const model::Kvnr& kvnr);
    void setDateTime(const model::Timestamp& dateTime);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Consent>;
}

#endif
