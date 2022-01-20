/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/admin/AdminServiceContext.hxx"
#include "erp/enrolment/EnrolmentServiceContext.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"


template class RequestHandlerInterface<PcServiceContext>;
template class RequestHandlerBasicAuthentication<EnrolmentServiceContext>;
template class RequestHandlerBasicAuthentication<AdminServiceContext>;

template<class ServiceContextType>
void RequestHandlerBasicAuthentication<ServiceContextType>::handleBasicAuthentication(
    const SessionContext<ServiceContextType>& session, ConfigurationKey credentialsKey,
    ConfigurationKey debugDisableAuthenticationKey) const
{
    if (! Configuration::instance().getOptionalBoolValue(debugDisableAuthenticationKey, false))
    {
        SafeString secret(Configuration::instance().getSafeStringValue(credentialsKey));
        ErpExpect(session.request.header().hasHeader(Header::Authorization), HttpStatus::Unauthorized,
                  "Authorization required.");
        auto parts = String::split(session.request.header().header(Header::Authorization).value_or(""), " ");
        if (parts.size() == 2)
        {
            SafeString authSecret(std::move(parts[1]));
            ErpExpect(String::toLower(parts[0]) == "basic" && authSecret == secret, HttpStatus::Unauthorized,
                      "Unauthorized");
        }
        else
        {
            ErpExpect(false, HttpStatus::Unauthorized, "Unauthorized (unsupported authorization content.)");
        }
    }
}
