/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_CONSENTDECISIONSRESPONSETYPE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_CONSENTDECISIONSRESPONSETYPE_HXX

#include <optional>
#include <string>
#include <unordered_map>

namespace model
{

class ConsentDecisionsResponseType
{
public:
    using FunctionIdType = std::string;
    enum class Decision : u_int8_t
    {
        permit,
        deny
    };

    explicit ConsentDecisionsResponseType(std::string_view json);

    std::optional<Decision> getDecisionFor(const FunctionIdType& functionId) const;

private:
    std::unordered_map<FunctionIdType, Decision> mFunctions;
};

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_CONSENTDECISIONSRESPONSETYPE_HXX
