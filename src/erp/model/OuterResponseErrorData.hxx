/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_OUTERRESPONSEERRORDATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_OUTERRESPONSEERRORDATA_HXX

#include "shared/network/message/HttpStatus.hxx"
#include "shared/model/ResourceBase.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class OuterResponseErrorData : public ResourceBase
{
public:
    OuterResponseErrorData(
        const std::string_view& xRequestId,
        HttpStatus status,
        const std::string_view& error,
        const std::optional<std::string_view>& message);

    [[nodiscard]] std::string_view xRequestId() const;
    [[nodiscard]] HttpStatus status() const;
    [[nodiscard]] std::string_view error() const;
    [[nodiscard]] std::optional<std::string_view> message() const;

private:
    void setXRequestId(const std::string_view& xRequestId);
    void setStatus(HttpStatus status);
    void setError(const std::string_view& error);
    void setMessage(const std::string_view& message);
};

}

#endif // ERP_PROCESSING_CONTEXT_MODEL_OUTERRESPONSEERRORDATA_HXX
