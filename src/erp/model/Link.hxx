/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_LINK_HXX
#define ERP_PROCESSING_CONTEXT_LINK_HXX

namespace model
{

/**
 * Helper functionality around RESTful links (self, prev, next).
 */
class Link
{
public:
    enum Type
    {
        Self,
        Prev,
        Next,
        First,
        Last
    };
};

} // end of namespace model

#endif
