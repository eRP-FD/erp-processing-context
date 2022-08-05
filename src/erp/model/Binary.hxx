/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_BINARY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_BINARY_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/PrescriptionId.hxx"

#include <rapidjson/document.h>

#include <optional>

namespace model
{

// Reduced version of Binary resource, contains only functionality currently needed;

class Binary : public Resource<Binary>
{
public:
    static constexpr auto resourceTypeName = "Binary";

    enum class Type
    {
        Base64,
        PKCS7
    };

    Binary(std::string_view id, std::string_view data, const Type type = Type::PKCS7);

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] std::optional<std::string_view> data() const;

private:
    friend Resource<Binary>;
    explicit Binary(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
