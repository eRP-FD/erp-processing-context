/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/ModelException.hxx"

namespace model {

ModelException::ModelException (const std::string& message)
    : std::runtime_error(message)
{
}

}