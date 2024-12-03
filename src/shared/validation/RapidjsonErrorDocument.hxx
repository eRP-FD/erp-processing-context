// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_RAPIDJSONERRORDOCUMENT_HXX
#define ERP_PROCESSING_CONTEXT_RAPIDJSONERRORDOCUMENT_HXX
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <optional>


// https://rapidjson.org/md_doc_schema.html#Reporting
// The parsing of the error document could be incomplete, and should be enhanced when encountering errors not
// covered by this implementation. Furthermore the document format is marked as instable by rapidjson, and could
// change over time.
class RapidjsonErrorDocument
{
public:
    using ValueType = rapidjson::GenericSchemaValidator<
        rapidjson::GenericSchemaDocument<
            rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>,
            rapidjson::CrtAllocator>,
        rapidjson::BaseReaderHandler<rapidjson::UTF8<char>, void>, rapidjson::CrtAllocator>::ValueType;

    explicit RapidjsonErrorDocument(ValueType& value);

    [[nodiscard]] const std::optional<ValueType>& firstError() const;

private:
    bool iterateMember(ValueType& mem);
    bool iterateArray(ValueType& mem);

    std::optional<ValueType> firstError_;
};


#endif//ERP_PROCESSING_CONTEXT_RAPIDJSONERRORDOCUMENT_HXX
