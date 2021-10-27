/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTMODEL_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTMODEL_HXX

#include "erp/util/SafeString.hxx"
#include "erp/hsm/ErpTypes.hxx"

#include <rapidjson/document.h>
#include <string_view>
#include <vector>


/**
 * This is really only a small wrapper around a rapidjson document with convenience methods for reading and
 * writing values.
 */
class EnrolmentModel
{
public:
    void set (std::string_view key, std::string_view stringValue);
    void set (std::string_view key, int64_t numberValue);

    std::string getString (std::string_view key) const;
    std::optional<std::string> getOptionalDecodedString (std::string_view key) const;
    std::string getDecodedString (std::string_view key) const;
    SafeString getSafeString (std::string_view key) const;
    int64_t getInt64 (std::string_view key) const;
    std::vector<size_t> getSizeTVector (std::string_view key) const;
    std::optional<std::vector<size_t>> getOptionalSizeTVector (std::string_view key) const;
    ErpVector getErpVector (std::string_view key) const;
    ErpVector getDecodedErpVector (std::string_view key) const;

    EnrolmentModel (void);
    explicit EnrolmentModel (std::string_view jsonText);
    std::string serializeToString (void) const;

private:
    rapidjson::Document mDocument;

    const rapidjson::Value& getValue (std::string_view key) const;
    const rapidjson::Value* getOptionalValue (std::string_view key) const;
};


#endif
