/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SUBSCRIPTIONPOSTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SUBSCRIPTIONPOSTHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "erp/model/TelematikId.hxx"

class PcServiceContext;
class SessionContext;

class SubscriptionPostHandler : public ErpRequestHandler
{
public:
    SubscriptionPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest(PcSessionContext& session) override;

    static void publish(SessionContext& serviceContext, const model::TelematikId& recipient);
};

#endif// #ifndef ERP_PROCESSING_CONTEXT_SUBSCRIPTIONPOSTHANDLER_HXX
