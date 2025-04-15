/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/search/SortArgument.hxx"

#include <ostream>


SortArgument::SortArgument (const std::string& name)
    : nameUrl(name[0] == sortOrderKey ? name.substr(1) : name),
      nameDb(nameUrl), // can be changed later in UrlArguments
      order(name[0] == sortOrderKey ? Order::Decreasing : Order::Increasing)
{
}


void SortArgument::appendLinkString (std::ostream& os) const
{
    if (order == Order::Decreasing)
        os << '-';
    os << nameUrl;
}
