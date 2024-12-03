/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_METADATA_METADATAVALIDATIONERROR_HXX
#define EPA_LIBRARY_METADATA_METADATAVALIDATIONERROR_HXX

#include <stdexcept>

namespace epa
{

/**
 * Indicates an error in validating metadata. This is an internal exception class for library and
 * backend. On client side we have the ValidationError class.
 */
class MetadataValidationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
} //  namespace epa

#endif
