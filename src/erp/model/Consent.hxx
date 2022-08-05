/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_CONSENT_HXX

#include "erp/model/Resource.hxx"
#include "fhirtools/model/Timestamp.hxx"

#include <optional>

namespace model
{

class Consent : public Resource<Consent>
{
public:
    static constexpr auto resourceTypeName = "Consent";
    enum class Type
    {
        CHARGCONS
    };

    static std::string createIdString(Consent::Type type, const std::string_view& kvnr);
    static std::pair<Consent::Type, std::string> splitIdString(const std::string_view& idStr);

    // The resource id field is filled by the constructor:
    Consent(
        const std::string_view& kvnr,
        const fhirtools::Timestamp& dateTime);

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] std::string_view patientKvnr() const;
    [[nodiscard]] bool isChargingConsent() const;
    [[nodiscard]] fhirtools::Timestamp dateTime() const;

    void fillId();

private:
    friend Resource<Consent>;
    explicit Consent(NumberAsStringParserDocument&& jsonTree);

    void setId(const std::string_view& id);
    void setPatientKvnr(const std::string_view& kvnr);
    void setDateTime(const fhirtools::Timestamp& dateTime);
};

}


#endif
