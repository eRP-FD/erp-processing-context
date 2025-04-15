/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_EPA_RECORDSTATE_HXX
#define EPA_LIBRARY_EPA_RECORDSTATE_HXX

#include "library/util/EnumBase.hxx"

#include <string>
namespace epa
{

/**
 * The state in which the insurant's health record account can be.
 *
 * The authorization service keeps track of this state, and it is contained in the
 * returned authorization assertion on the insurant's authorization request.
 * See: gemSpec_Aktensystem, section 6.1.1 "Kontoverwaltung und Zustandswechsel".
 *
 * Compare with wsdl/fd/phr/AuthorizationService.xsd
 * \<complexType name="RecordStateType">
 */
class RecordState : public EnumBase<RecordState>
{
public:
    EPA_VALUES_ENUM(
        UNKNOWN,
        REGISTERED,
        REGISTERED_FOR_MIGRATION,
        ACTIVATED,
        DISMISSED,
        SUSPENDED,
        KEY_CHANGE,
        DL_IN_PROGRESS,
        READY_FOR_IMPORT,
        START_MIGRATION,
        DELETION_IN_PROGRESS // Not valid for AuthZ but for RAM.
    );

    // NOLINTNEXTLINE: Make this explicit(false) when we can use C++20.
    RecordState(Value value = UNKNOWN);

    Value getValue() const;

    const std::string& toString() const;

    static RecordState fromString(const std::string& string);

    const std::string& toJsonString() const;

    static RecordState fromJsonString(const std::string& string);

private:
    Value mValue;
};
} // namespace epa

#endif
