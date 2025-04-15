/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX

#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

// Reduced version of Device resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class MetaData : public Resource<MetaData>
{
public:
    static constexpr auto resourceTypeName = "CapabilityStatement";
    static constexpr auto profileType = ProfileType::fhir;

    explicit MetaData(const model::Timestamp& referenceTime = model::Timestamp::now());

    [[nodiscard]] model::Timestamp date() const;
    [[nodiscard]] std::string_view version() const;
    [[nodiscard]] model::Timestamp releaseDate() const;

    void setDate(const model::Timestamp& date);
    void setVersion(const std::string_view& version);
    void setReleaseDate(const model::Timestamp& releaseDate);

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend class Resource;
    explicit MetaData(NumberAsStringParserDocument&& jsonTree);

    template<class TemplateDocument>
    void addResourceTemplate(const TemplateDocument& templateDocument);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<MetaData>;
}


#endif
