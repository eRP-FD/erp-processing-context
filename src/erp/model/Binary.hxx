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
    explicit Binary(std::string_view id, std::string_view data);

    [[nodiscard]] std::optional<std::string_view> id() const;
    [[nodiscard]] std::optional<std::string_view> data() const;

private:
    friend Resource<Binary>;
    explicit Binary(NumberAsStringParserDocument&& jsonTree);
};

}


#endif
