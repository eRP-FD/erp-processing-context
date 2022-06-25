/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

//NOLINTNEXTLINE[bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp]
#define _USE_CUSTOM_READER

#include "test/erp/model/JsonMaintainNumberPrecisionTestModel.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"

#include <boost/format.hpp>

#include <map>

using namespace model;
using namespace model::resource;

namespace rj = rapidjson;

namespace {
const rj::Pointer resourceTypePointer(ElementName::path(elements::resourceType));
const rj::Pointer statusPointer(ElementName::path(elements::status));
const rj::Pointer sentPointer(ElementName::path(elements::sent));
}

JsonMaintainNumberPrecisionTestModel JsonMaintainNumberPrecisionTestModel::fromJson(std::string& jsonString)
{
    return JsonMaintainNumberPrecisionTestModel(NumberAsStringParserDocument::fromJson(jsonString));
}

JsonMaintainNumberPrecisionTestModel::JsonMaintainNumberPrecisionTestModel(NumberAsStringParserDocument&& document) :
    ResourceBase(std::move(document))
{
}

std::string JsonMaintainNumberPrecisionTestModel::serializeToJsonString() const
{
    return jsonDocument().serializeToJsonString();
}

std::optional<Timestamp> JsonMaintainNumberPrecisionTestModel::timeSent() const
{
    const auto optionalValue = getOptionalStringValue(sentPointer);
    if (optionalValue.has_value())
        return Timestamp::fromFhirDateTime(std::string(optionalValue.value()));
    else
        return {};
}

void JsonMaintainNumberPrecisionTestModel::setTimeSent(const Timestamp& timestamp)
{
    ResourceBase::setValue(sentPointer, timestamp.toXsDateTime());
}

std::string_view JsonMaintainNumberPrecisionTestModel::getValueAsString(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getStringValue(rj::Pointer(path));
}

int JsonMaintainNumberPrecisionTestModel::getValueAsInt(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getIntValue(rj::Pointer(path));
}

unsigned int JsonMaintainNumberPrecisionTestModel::getValueAsUInt(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getUIntValue(rj::Pointer(path));
}

int64_t JsonMaintainNumberPrecisionTestModel::getValueAsInt64(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getInt64Value(rj::Pointer(path));
}

uint64_t JsonMaintainNumberPrecisionTestModel::getValueAsUInt64(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getUInt64Value(rj::Pointer(path));
}

double JsonMaintainNumberPrecisionTestModel::getValueAsDouble(const std::string& name) const
{
    std::string path = ElementName::path(name);
    return getDoubleValue(rj::Pointer(path));
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, const std::string_view& value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, int value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, unsigned int value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, int64_t value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, uint64_t value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}

void JsonMaintainNumberPrecisionTestModel::setValue(const std::string& name, double value)
{
    std::string path = ElementName::path(name);
    ResourceBase::setValue(rj::Pointer(path), value);
}
