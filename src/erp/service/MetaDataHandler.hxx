/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_METADATAHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_METADATAHANDLER_HXX


#include "erp/service/ErpRequestHandler.hxx"


class MetaDataHandler
    : public ErpUnconstrainedRequestHandler
{
public:
    MetaDataHandler();
    void handleRequest (PcSessionContext& session) override;
};


#endif
