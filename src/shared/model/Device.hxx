/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_DEVICE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_DEVICE_HXX

#include "shared/model/Resource.hxx"

#include <optional>

namespace model
{

// Reduced version of Device resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class Device : public Resource<Device>
{
public:
    static constexpr auto resourceTypeName = "Device";
    static constexpr auto profileType = ProfileType::Gem_erxDevice;

    static constexpr uint16_t Id = 1;
    static constexpr std::string_view Name = "E-Rezept Fachdienst";
    static constexpr std::string_view Email = "betrieb@gematik.de";
    enum class Status {
        active,
        inactive,
        entered_in_error,
        unknown
    };
    enum class CommunicationSystem {
        phone,
        fax,
        email,
        pager,
        url,
        sms,
        other
    };
    Device();

    [[nodiscard]] std::string_view id() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] std::string_view version() const;
    [[nodiscard]] std::string_view serialNumber() const;
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] std::optional<std::string_view> contact(CommunicationSystem system) const;

    void setId(const std::string_view& id);
    void setStatus(Status status);
    void setVersion(const std::string_view& version);
    void setSerialNumber(const std::string_view& serialNumber);
    void setName(const std::string_view& name);
    void setContact(CommunicationSystem system, const std::string_view& contact);

    static std::string createReferenceUrl(const std::string& linkBase);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;


private:
    friend Resource<Device>;
    explicit Device(NumberAsStringParserDocument&& jsonTree);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<Device>;
}


#endif
