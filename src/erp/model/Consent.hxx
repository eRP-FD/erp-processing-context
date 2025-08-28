/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX

#include "shared/model/AuditData.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>

namespace model
{
enum class ConsentType
{
    CHARGCONS,
    EUDISPCONS
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class Consent : public Resource<Consent>
{
public:
    static constexpr auto resourceTypeName = "Consent";
    ProfileType profileType() const;
    static ProfileType profileType(ConsentType type);
    static constexpr auto chargingConsentType = magic_enum::enum_name(ConsentType::CHARGCONS);
    static constexpr auto euDispenseConsentType = magic_enum::enum_name(ConsentType::EUDISPCONS);

    static std::string createIdString(ConsentType type, const Kvnr& kvnr);
    static std::string createIdString(AuditEventId auditEventId, const Kvnr& kvnr);
    static std::pair<ConsentType, std::string> splitIdString(const std::string_view& idStr);

    // The resource id field is filled by the constructor:
    Consent(ConsentType category, const model::Kvnr& kvnr, const model::Timestamp& dateTime);

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] Kvnr patientKvnr() const;
    [[nodiscard]] bool isChargingConsent() const;
    [[nodiscard]] model::Timestamp dateTime() const;
    [[nodiscard]] ConsentType consentCategory() const;

    void fillId(ConsentType category, const Kvnr& kvnr);
    void fillCategory(ConsentType category);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend Resource<Consent>;
    explicit Consent(NumberAsStringParserDocument&& jsonTree);

    void setPatientKvnr(const model::Kvnr& kvnr);
    void setDateTime(const model::Timestamp& dateTime);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Consent>;
}

#endif
