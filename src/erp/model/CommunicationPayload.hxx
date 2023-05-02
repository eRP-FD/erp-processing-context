/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX

#include "erp/model/CommunicationPayloadItem.hxx"

#include <memory>
#include <vector>

namespace model
{
class CommunicationPayload
{
public:
    static const size_t maxPayloadSize = 10240;

    explicit CommunicationPayload(const rapidjson::Value* payloadValue);

    void verifyLength() const;

private:
    std::vector<std::unique_ptr<CommunicationPayloadItem>> mItems;
};

} // namespace model

#endif
