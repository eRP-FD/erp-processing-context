/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Extension.txx"

#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/pointer.h>

using model::Extension;


const rapidjson::Pointer model::ExtensionBase::valuePeriodStartPointer(
    model::resource::ElementName::path(model::resource::elements::valuePeriod, model::resource::elements::start));
const rapidjson::Pointer model::ExtensionBase::valuePeriodEndPointer(
    model::resource::ElementName::path(model::resource::elements::valuePeriod, model::resource::elements::end));

template class model::Extension<model::UnspecifiedExtension>;
