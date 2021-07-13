#ifndef ERP_PROCESSING_CONTEXT_PARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_PARAMETER_HXX

#include "erp/model/PrescriptionType.hxx"
#include "erp/model/Resource.hxx"

#include <string_view>
#include <libxml2/libxml/tree.h>
#include <memory>



namespace model
{
class Parameters : public Resource<Parameters>
{
public:
    size_t count() const;
    const rapidjson::Value* findResourceParameter(const std::string_view& name) const;

    // Available in Task/$create request parameter
    std::optional<model::PrescriptionType> getPrescriptionType() const;

private:
    friend Resource<Parameters>;
    explicit Parameters (NumberAsStringParserDocument&& document);

};


}

#endif //ERP_PROCESSING_CONTEXT_PARAMETER_HXX
