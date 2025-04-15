/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODELEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_MODELEXCEPTION_HXX


#include <stdexcept>

namespace model {

class ModelException : public std::runtime_error
{
public:
    explicit ModelException (const std::string& message);
};


}

#endif
