/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SORTARGUMENT_HXX
#define ERP_PROCESSING_CONTEXT_SORTARGUMENT_HXX

#include <string>


/**
 * A sort argument. Extracted from a URL. For the eRezept, sorting is only supported for searchable parameters.
 * Expected in the form
 *     _sort=<value>[,<value>]*
 * where value is the name of a search parameter, optionally prefix with '-' to invert sort direction which defaults to
 * "increasing".
 *
 * See https://www.hl7.org/fhir/search.html#_sort for details.
 */

class SortArgument
{
public:
    static constexpr std::string_view sortKey = "_sort";
    static constexpr char sortOrderKey = '-';
    static constexpr char argumentSeparator = ',';

    enum class Order
    {
        Increasing,
        Decreasing
    };

    const std::string nameUrl;
    std::string nameDb;
    const Order order;

    /**
     * Create a new SortArgument from a single parameter.
     *
     * @param name   Can be prefixed with a '-' to reverse sort order (to decreasing). Must not be empty.
     */
    explicit SortArgument (const std::string& name);

    void appendLinkString (std::ostream& os) const;
};


#endif
