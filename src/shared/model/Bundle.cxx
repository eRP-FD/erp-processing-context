/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Bundle.hxx"

namespace model
{

std::optional<Timestamp> Bundle::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

} // end of namespace model
