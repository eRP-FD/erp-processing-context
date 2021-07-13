#ifndef ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"

class CadesBesSignature;
class TslManager;

class ActivateTaskHandler : public TaskHandlerBase
{
public:
    ActivateTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;

private:
    /**
     * Allows to unpack CAdES-BES signature, for testing purpose it is allowed to provide no TslManager.
     */
    static CadesBesSignature unpackCadesBesSignature(
        const std::string& pkcs7File, TslManager* tslManager);
};



#endif //ERP_PROCESSING_CONTEXT_ACTIVATETASKHANDLER_HXX
