/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_COMMUNICATIONDELETEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_COMMUNICATIONDELETEHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"

namespace model
{
class Communication;
}

class CommunicationDeleteHandler : public ErpRequestHandler
{
public:
    CommunicationDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);
    void handleRequest(PcSessionContext& session) override;
};

#endif //ERP_PROCESSING_CONTEXT_COMMUNICATIONDELETEHANDLER_HXX
