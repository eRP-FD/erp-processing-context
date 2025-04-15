/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXPORTER_SHARED_DATABASE_ACCESSTOKENIDENTITY_H
#define ERP_PROCESSING_CONTEXT_EXPORTER_SHARED_DATABASE_ACCESSTOKENIDENTITY_H

#include "shared/model/TelematikId.hxx"

#include <string>
#include <vector>

class ErpVector;
class SafeString;
class JWT;

namespace db_model
{

class AccessTokenIdentity
{
public:
    explicit AccessTokenIdentity(const JWT& jwt);
    AccessTokenIdentity(model::TelematikId id, std::string_view name, std::string_view oid);

    std::string getJson() const;
    const model::TelematikId& getId() const;
    const std::string& getName() const;
    const std::string& getOid() const;

    static AccessTokenIdentity fromJson(const std::string& json);

private:
    model::TelematikId mId;
    std::string mName;
    std::string mOid;
};

}// namespace db_model


#endif// ERP_PROCESSING_CONTEXT_EXPORTER_SHARED_DATABASE_ACCESSTOKENIDENTITY_H
