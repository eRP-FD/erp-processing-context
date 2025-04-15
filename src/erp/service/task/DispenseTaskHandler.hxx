/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TASK_DISPENSETASKHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_TASK_DISPENSETASKHANDLER_HXX

#include "erp/service/MedicationDispenseHandlerBase.hxx"

class DispenseTaskHandler : public MedicationDispenseHandlerBase
{
public:
    DispenseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest (PcSessionContext& session) override;

private:
};

#endif
