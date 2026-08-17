/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/ModelException.hxx"

namespace model
{

ModelException::ModelException(const std::string& message)
    : std::runtime_error(message)
{
}

MissingParameterException::MissingParameterException(const std::string& message)
    : ModelException(message)
{
}

InvalidParameterException::InvalidParameterException(const std::string& message)
    : ModelException(message)
{
}

}
