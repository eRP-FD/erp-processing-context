/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_CLOSETASKHANDLER_HXX

#include "erp/service/task/TaskHandler.hxx"
#include "shared/util/Configuration.hxx"
#include "erp/service/MedicationDispenseHandlerBase.hxx"

namespace fhirtools
{
class Collection;
}

class ErpElement;

class CloseTaskHandler
    : public MedicationDispenseHandlerBase
{
public:
    CloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;

private:
    static std::string generateCloseTaskDeviceRef(Configuration::DeviceRefType refType, const std::string& linkBase);

};

#endif
