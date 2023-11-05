/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_TELEMATIKID_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_TELEMATIKID_HXX

#include <string>
#include <string_view>

namespace model {


class TelematikId
{
public:
    explicit TelematikId(std::string_view telematikId);

    bool validFormat() const;

    const std::string& id() const&;
    /** Moves stored value */
    std::string id() &&;

    static bool isTelematikId(std::string_view value);
private:
    std::string mId;
};

bool operator==(const TelematikId& lhs, const TelematikId& rhs);
bool operator==(const TelematikId& lhs, std::string_view rhs);

} // namespace model

#endif // ERP_PROCESSING_CONTEXT_MODEL_TELEMATIKID_HXX