/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/DataAbsentReason.hxx"
#include "shared/model/ResourceNames.hxx"

namespace
{
const rapidjson::Pointer urlPtr(model::resource::ElementName::path(model::resource::elements::url));
const rapidjson::Pointer valueCodePtr(model::resource::ElementName::path(model::resource::elements::valueCode));
}

namespace model
{

DataAbsentReason::DataAbsentReason(const std::string_view valueCode)
    : Extension<DataAbsentReason>()
{
    setValue(urlPtr, url);
    setValue(valueCodePtr, valueCode);
}

template class Extension<DataAbsentReason>;

}
