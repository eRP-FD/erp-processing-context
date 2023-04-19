/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX

#include <rapidjson/document.h>


namespace model
{
class CommunicationPayload
{
public:
    static const std::size_t maxPayloadSize = 10240;

    explicit CommunicationPayload(const rapidjson::Value* payloadValue);

    void verifyLength() const;

private:
    std::size_t mLength = 0;
};

} // namespace model

#endif
