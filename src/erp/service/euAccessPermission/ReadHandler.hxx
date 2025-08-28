// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "erp/service/ErpRequestHandler.hxx"

namespace eu_access_permission
{

class ReadHandler : public ErpRequestHandler
{
public:
    ReadHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);

    void handleRequest(PcSessionContext& session) override;
};

}// euAccessPermission
