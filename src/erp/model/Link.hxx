/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
        Next
    };
};

} // end of namespace model

#endif
