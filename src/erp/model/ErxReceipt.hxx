#ifndef ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX

#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/Bundle.hxx"

#include "erp/util/Uuid.hxx"

#include <rapidjson/document.h>

namespace model
{

// Reduced version of ErxReceipt resource, contains only functionality currently needed;

class ErxReceipt : public Bundle
{
public:
    ErxReceipt(
        const Uuid& bundleId,
        const std::string& selfLink,
        const model::PrescriptionId& prescriptionId,
        const model::Composition& composition,
        const std::string& deviceIdentifier,
        const model::Device& device);

    // Converts numbers as strings and inserts prefixes to distinguish between numbers and strings.
    static ErxReceipt fromJson(const std::string_view& jsonStr);

    static ErxReceipt fromJson(const rapidjson::Value& json);   // Expects a json document with prefixed to distinguish between numbers and strings.
    static ErxReceipt fromXml(const std::string& xmlStr);

    model::PrescriptionId prescriptionId() const;
    model::Composition composition() const;
    model::Device device() const;

private:
    explicit ErxReceipt(NumberAsStringParserDocument&& jsonTree);
};

}

#endif
