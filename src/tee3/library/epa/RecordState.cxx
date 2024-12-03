/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/epa/RecordState.hxx"
#include "library/json/JsonReader.hxx"
#include "library/json/JsonWriter.hxx"

namespace epa
{

RecordState::RecordState(Value value)
  : mValue(value)
{
}


RecordState::Value RecordState::getValue() const
{
    return mValue;
}


const std::string& RecordState::toString() const
{
    return getTable().toString(*this);
}


RecordState RecordState::fromString(const std::string& string)
{
    return getTable().fromString(string);
}


const std::string& RecordState::toJsonString() const
{
    return getTable().toString(*this);
}


RecordState RecordState::fromJsonString(const std::string& string)
{
    return getTable().fromString<JsonError>(string);
}
} // namespace epa
