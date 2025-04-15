/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOAD_HXX

#include "shared/validation/SchemaType.hxx"

#include <rapidjson/document.h>

class JsonValidator;

namespace model
{
class CommunicationPayload
{
public:
    static constexpr std::size_t maxPayloadSize = 10240;

    explicit CommunicationPayload(const rapidjson::Value* payloadValue);

    void verifyLength() const;
    void validateJsonSchema(const JsonValidator& validator, SchemaType schemaType) const;
    [[nodiscard]] bool empty() const;

private:
    std::size_t mLength = 0;
    const rapidjson::Value* mPayloadValue = nullptr;
};

} // namespace model

#endif
