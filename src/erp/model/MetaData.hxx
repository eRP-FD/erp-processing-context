/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_METADATA_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>

#include <optional>

namespace model
{

// Reduced version of Device resource, contains only functionality currently needed;

class MetaData : public Resource<MetaData>
{
public:
    explicit MetaData();

    [[nodiscard]] model::Timestamp date() const;
    [[nodiscard]] std::string_view version() const;
    [[nodiscard]] model::Timestamp releaseDate() const;

    void setDate(const Timestamp& date);
    void setVersion(const std::string_view& version);
    void setReleaseDate(const Timestamp& releaseDate);

private:
    friend Resource<MetaData>;
    explicit MetaData(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
