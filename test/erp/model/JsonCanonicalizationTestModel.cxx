#define _USE_CUSTOM_READER

#include "test/erp/model/JsonCanonicalizationTestModel.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"

#include <boost/format.hpp>

#include <map>

using namespace model;
using namespace model::resource;

namespace rj = rapidjson;

const rj::Pointer JsonCanonicalizationTestModel::resourceTypePointer(ElementName::path(elements::resourceType));
const rj::Pointer JsonCanonicalizationTestModel::statusPointer(ElementName::path(elements::status));
const rj::Pointer JsonCanonicalizationTestModel::sentPointer(ElementName::path(elements::sent));
const rj::Pointer JsonCanonicalizationTestModel::valuePointer(ElementName::path(elements::value));

const std::map<JsonCanonicalizationTestModel::Status, std::string> JsonCanonicalizationTestModelStatusToString = {
    { JsonCanonicalizationTestModel::Status::Preparation,    "preparation"      },
    { JsonCanonicalizationTestModel::Status::InProgress,     "in-progress"      },
    { JsonCanonicalizationTestModel::Status::Cancelled,      "cancelled"        },
    { JsonCanonicalizationTestModel::Status::OnHold,         "on-hold"          },
    { JsonCanonicalizationTestModel::Status::Completed,      "completed"        },
    { JsonCanonicalizationTestModel::Status::EnteredInError, "entered-in-error" },
    { JsonCanonicalizationTestModel::Status::Stopped,        "stopped"          },
    { JsonCanonicalizationTestModel::Status::Declined,       "declined"         },
    { JsonCanonicalizationTestModel::Status::Unknown,        "unknown"          }
};

const std::map<std::string, JsonCanonicalizationTestModel::Status> JsonCanonicalizationTestModelStringToStatus = {
    { "preparation",      JsonCanonicalizationTestModel::Status::Preparation    },
    { "in-progress",      JsonCanonicalizationTestModel::Status::InProgress     },
    { "cancelled",        JsonCanonicalizationTestModel::Status::Cancelled      },
    { "on-hold",          JsonCanonicalizationTestModel::Status::OnHold         },
    { "completed",        JsonCanonicalizationTestModel::Status::Completed      },
    { "entered-in-error", JsonCanonicalizationTestModel::Status::EnteredInError },
    { "stopped",          JsonCanonicalizationTestModel::Status::Stopped        },
    { "declined",         JsonCanonicalizationTestModel::Status::Declined       },
    { "unknown",          JsonCanonicalizationTestModel::Status::Unknown        }
};

const std::string& JsonCanonicalizationTestModel::statusToString(Status status)
{
    const auto& it = JsonCanonicalizationTestModelStatusToString.find(status);
    Expect3(it != JsonCanonicalizationTestModelStatusToString.end(), "Status enumerator value " + std::to_string(static_cast<int>(status)) + " is out of range", std::logic_error);
    return it->second;
}

JsonCanonicalizationTestModel::Status JsonCanonicalizationTestModel::stringToStatus(const std::string& status)
{
    const auto& it = JsonCanonicalizationTestModelStringToStatus.find(status);
    ModelExpect(it != JsonCanonicalizationTestModelStringToStatus.end(), "Invalid status " + std::string(status));
    return it->second;
}

JsonCanonicalizationTestModel JsonCanonicalizationTestModel::fromJson(std::string& jsonString)
{
    NumberAsStringParserDocument jsonDoc;
    rj::StringStream s(jsonString.data());
    jsonDoc.ParseStream<rapidjson::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s);

    ModelExpect(!jsonDoc.HasParseError(), "can not parse json string");

    return JsonCanonicalizationTestModel(std::move(jsonDoc));
}

JsonCanonicalizationTestModel::JsonCanonicalizationTestModel(NumberAsStringParserDocument&& document) :
    ResourceBase(std::move(document))
{
}

std::optional<JsonCanonicalizationTestModel::Status> JsonCanonicalizationTestModel::status() const
{
    std::optional<std::string_view> status = getOptionalStringValue(statusPointer);
    if (status.has_value())
    {
        return stringToStatus(std::string(status.value()));
    }
    return {};
}

void JsonCanonicalizationTestModel::setStatus(Status status)
{
    setValue(statusPointer, statusToString(status));
}

std::optional<Timestamp> JsonCanonicalizationTestModel::timeSent() const
{
    const auto optionalValue = getOptionalStringValue(sentPointer);
    if (optionalValue.has_value())
        return Timestamp::fromFhirDateTime(std::string(optionalValue.value()));
    else
        return {};
}

void JsonCanonicalizationTestModel::setTimeSent(const Timestamp& timestamp)
{
    setValue(sentPointer, timestamp.toXsDateTime());
}
