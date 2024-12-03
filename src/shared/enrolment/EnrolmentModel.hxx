/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTMODEL_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTMODEL_HXX

#include "shared/util/SafeString.hxx"
#include "shared/hsm/ErpTypes.hxx"

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
    void set(std::string_view key, const rapidjson::Value& value);

    bool hasValue(std::string_view key) const;

    std::string getString (std::string_view key) const;
    std::optional<std::string> getOptionalString (std::string_view key) const;
    std::optional<std::string> getOptionalDecodedString (std::string_view key) const;
    std::string getDecodedString (std::string_view key) const;
    SafeString getSafeString (std::string_view key) const;
    int64_t getInt64 (std::string_view key) const;
    ::std::optional<int64_t> getOptionalInt64(::std::string_view key) const;
    std::vector<size_t> getSizeTVector (std::string_view key) const;
    std::optional<std::vector<size_t>> getOptionalSizeTVector (std::string_view key) const;
    ErpVector getErpVector (std::string_view key) const;
    ErpVector getDecodedErpVector (std::string_view key) const;

    EnrolmentModel (void);
    explicit EnrolmentModel (std::string_view jsonText);
    EnrolmentModel(EnrolmentModel&&) = default;
    std::string serializeToString (void) const;
    std::string serializeToString(std::string_view key) const;

    rapidjson::Document& document();

    virtual ~EnrolmentModel() = default;

protected:
    rapidjson::Document mDocument;

    void parse(std::string_view json);
    const rapidjson::Value& getValue (std::string_view key) const;
    const rapidjson::Value* getOptionalValue (std::string_view key) const;
};


#endif
