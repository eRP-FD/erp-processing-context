/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

// Reduced version of Device resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class MetaData : public Resource<MetaData, ResourceVersion::NotProfiled>
{
public:
    static constexpr auto resourceTypeName = "CapabilityStatement";

    explicit MetaData(ResourceVersion::FhirProfileBundleVersion profileBundle);

    [[nodiscard]] model::Timestamp date() const;
    [[nodiscard]] std::string_view version() const;
    [[nodiscard]] model::Timestamp releaseDate() const;

    void setDate(const model::Timestamp& date);
    void setVersion(const std::string_view& version);
    void setReleaseDate(const model::Timestamp& releaseDate);

    ResourceVersion::DeGematikErezeptWorkflowR4 taskProfileVersion();

private:
    friend class Resource;
    explicit MetaData(NumberAsStringParserDocument&& jsonTree);

    template<class ProfileDefinition>
    void fillResource(const ProfileDefinition& profileDefinition,
                      ResourceVersion::FhirProfileBundleVersion profileBundle);

    template<class TemplateDocument>
    void addResourceTemplate(const TemplateDocument& templateDocument);
};

}


#endif
