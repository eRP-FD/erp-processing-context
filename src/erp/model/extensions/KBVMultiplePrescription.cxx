/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/extensions/KBVMultiplePrescription.hxx"

#include "erp/util/TLog.hxx"


using model::KBVMultiplePrescription;

bool model::KBVMultiplePrescription::isMultiplePrescription() const
{
    auto kennzeichen = getExtension<Kennzeichen>();
    std::optional<bool> res = kennzeichen ? kennzeichen->valueBoolean() : std::nullopt;
    return res && res.value();
}
