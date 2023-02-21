/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_IDENTITY_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_IDENTITY_HXX

#include "erp/model/Kvnr.hxx"
#include "erp/model/TelematikId.hxx"

#include <variant>

namespace model
{

using Identity = std::variant<model::Kvnr, model::TelematikId>;
inline const std::string& getIdentityString(const Identity& identity)
{
    return std::holds_alternative<model::Kvnr>(identity) ? std::get<model::Kvnr>(identity).id()
                                                         : std::get<model::TelematikId>(identity).id();
}

} // namespace model

#endif // ERP_PROCESSING_CONTEXT_MODEL_IDENTITY_HXX