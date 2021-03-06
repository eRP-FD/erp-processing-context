/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_SIGNATURE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_SIGNATURE_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>

#include <optional>

class MimeType;

namespace model
{

// Reduced version of Signature resource, contains only functionality currently needed;

class Signature : public Resource<Signature>
{
public:
    constexpr static const std::string_view jwsSystem = "urn:iso-astm:E1762-95:2013";
    constexpr static const std::string_view jwsCode = "1.2.840.10065.1.12.1.5";

    Signature(
        const std::string_view& data,
        const model::Timestamp& when,
        const std::string_view& who);

    [[nodiscard]] std::optional<std::string_view> data() const;
    [[nodiscard]] std::optional<model::Timestamp> when() const;
    [[nodiscard]] std::optional<std::string_view> who() const;
    [[nodiscard]] std::optional<MimeType> sigFormat() const;
    [[nodiscard]] std::optional<MimeType> targetFormat() const;

    void setTargetFormat(const MimeType& format);
    void setSigFormat(const MimeType& format);
    void setType(std::string_view system, std::string_view code);

private:
    friend Resource<Signature>;
    explicit Signature(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
