/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_COMMUNICATIONGETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_COMMUNICATIONGETHANDLER_HXX

#include "erp/model/Communication.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "shared/model/Bundle.hxx"


class Database;
class JWT;


class CommunicationGetHandlerBase : public ErpRequestHandler
{
public:
    explicit CommunicationGetHandlerBase (Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void markCommunicationsAsRetrieved (
        Database& database,
        std::vector<model::Communication>& communications,
        const std::string& caller);
    model::Bundle createBundle (
        const std::vector<model::Communication>& communications) const;
    std::string getValidatedCaller (PcSessionContext& session) const;
};


class CommunicationGetAllHandler
    : public CommunicationGetHandlerBase
{
public:
    CommunicationGetAllHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;
};


class CommunicationGetByIdHandler
    : public CommunicationGetHandlerBase
{
public:
    CommunicationGetByIdHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;

private:
    Uuid getValidatedCommunicationId (PcSessionContext& session) const;
};

#endif
