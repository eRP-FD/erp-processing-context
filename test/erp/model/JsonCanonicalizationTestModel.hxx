/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONCANONICALIZATIONTESTMODEL_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_MODEL_JSONCANONICALIZATIONTESTMODEL_HXX

#include "erp/model/Resource.hxx"
#include "fhirtools/model/Timestamp.hxx"

/**
 * The test resource model was introduced in order to be able to test the most diverse aspects of
 * canonization and to be independent of the resource models of the base system. The test model
 * should also allow to incorporate very specific error cases or track bug reports.
 */
class JsonCanonicalizationTestModel : public model::ResourceBase
{
public:
    enum class Status {
        Preparation,
        InProgress,
        Cancelled,
        OnHold,
        Completed,
        EnteredInError,
        Stopped,
        Declined,
        Unknown
    };
    static const std::string& statusToString(Status status);
    static Status stringToStatus(const std::string& status);

    static const rapidjson::Pointer resourceTypePointer;
    static const rapidjson::Pointer statusPointer;
    static const rapidjson::Pointer valuePointer;
    static const rapidjson::Pointer sentPointer;

    static JsonCanonicalizationTestModel fromJson(std::string& jsonString);

    [[nodiscard]] std::optional<Status> status() const;
    void setStatus(Status status);

    [[nodiscard]] std::optional<fhirtools::Timestamp> timeSent() const;
    void setTimeSent(const fhirtools::Timestamp& timestamp = fhirtools::Timestamp::now());
private:
    explicit JsonCanonicalizationTestModel(model::NumberAsStringParserDocument&& document); // internal ctor
};

#endif
