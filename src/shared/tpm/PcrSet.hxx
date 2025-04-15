/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PCRSET_HXX
#define ERP_PROCESSING_CONTEXT_PCRSET_HXX

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <rapidjson/document.h>


/**
 * Represent a list of PCR registers. Not the content of the registers but their indices.
 * PCR0 - static - same value on all my readings
 * PCR1 - static per host (host specific)
 * PCR2 - it will show a certain value if pcrUpdateCounter=78, and another value if pcrUpdateCounter=80 (does not seem to be affected by host location, release or SB state)
 * PCR3 - static - same value on all my readings
 * PCR4 - static for same release and SB setting, it changes if either release of SB state changes
 * PCR5 - static - same value on all my readings
 * PCR6 - static - same value on all my readings
 * PCR7 - changes with SB state, and, most probably, other SB related settings.
 */
class PcrSet
{
public:
    /**
     * The default set is [0,3,4]
     * @return
     */
    static PcrSet defaultSet (void);

    static PcrSet fromString (const std::string& stringValue);
    static PcrSet fromList (const std::vector<size_t>& registers);
    static std::optional<PcrSet> fromJson (const rapidjson::Document& document, std::string_view pointerToPcrSetArray);

    /**
     * Convert to the format that is defined by the tpm client.
     */
    std::vector<size_t> toPcrList (void) const;

    std::string toString (bool addBrackets = true) const;

    bool operator== (const PcrSet& other) const;

    size_t size (void) const;

    using const_iterator = std::set<std::uint8_t>::const_iterator;
    const_iterator begin (void) const;
    const_iterator end (void) const;

private:
    std::set<std::uint8_t> mRegisters; // Each register has a range of [0,15].
};


#endif
