/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ModelException.hxx"

namespace model {

ModelException::ModelException (const std::string& message)
    : std::runtime_error(message)
{
}

}