/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_BINARY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_BINARY_HXX

#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Resource.hxx"

#include <rapidjson/document.h>
#include <optional>
#include <string_view>

namespace model
{

// Reduced version of Binary resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class Binary : public Resource<Binary>
{
public:
    static constexpr auto resourceTypeName = "Binary";

    enum class Type
    {
        Digest,
        PKCS7
    };

    Binary(std::string_view id, std::string_view data, const Type type = Type::PKCS7,
           const std::optional<std::string_view>& metaVersionId = "1");
    Binary(std::string_view id, std::string_view data, const Profile& profile, const Type type = Type::PKCS7,
           const std::optional<std::string_view>& metaVersionId = "1");

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] std::optional<std::string_view> data() const;

private:
    friend Resource<Binary>;
    explicit Binary(NumberAsStringParserDocument&& jsonTree);
};

extern template class Resource<Binary>;
}


#endif
