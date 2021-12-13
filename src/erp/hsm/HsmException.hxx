/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSMEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSMEXCEPTION_HXX

#include <stdexcept>


class HsmException : public std::runtime_error
{
public:
    const uint32_t errorCode;

    HsmException(const std::string& what, const uint32_t errorCode);
};


#endif
