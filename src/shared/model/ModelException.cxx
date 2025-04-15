/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/ModelException.hxx"

namespace model {

ModelException::ModelException (const std::string& message)
    : std::runtime_error(message)
{
}

}
