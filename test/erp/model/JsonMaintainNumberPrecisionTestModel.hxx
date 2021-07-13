#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONMAINTAINNUMBERPRECISIONTESTMODEL_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONMAINTAINNUMBERPRECISIONTESTMODEL_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/Timestamp.hxx"

class JsonMaintainNumberPrecisionTestModel : public model::ResourceBase
{
public:
    static JsonMaintainNumberPrecisionTestModel fromJson(std::string& jsonString);
    [[nodiscard]] std::string serializeToJsonString() const;

    [[nodiscard]] std::optional<model::Timestamp> timeSent() const;
    void setTimeSent(const model::Timestamp& timestamp = model::Timestamp::now());

    [[nodiscard]] std::string_view getValueAsString(const std::string& name) const;
    [[nodiscard]] int getValueAsInt(const std::string& name) const;
    [[nodiscard]] unsigned int getValueAsUInt(const std::string& name) const;
    [[nodiscard]] int64_t getValueAsInt64(const std::string& name) const;
    [[nodiscard]] uint64_t getValueAsUInt64(const std::string& name) const;
    [[nodiscard]] double getValueAsDouble(const std::string& name) const;

    void setValue(const std::string& name, const std::string_view& value);
    void setValue(const std::string& name, int number);
    void setValue(const std::string& name, unsigned int number);
    void setValue(const std::string& name, int64_t number);
    void setValue(const std::string& name, uint64_t number);
    void setValue(const std::string& name, double number);
private:
    explicit JsonMaintainNumberPrecisionTestModel(model::NumberAsStringParserDocument&& jsonTree);
};

#endif
