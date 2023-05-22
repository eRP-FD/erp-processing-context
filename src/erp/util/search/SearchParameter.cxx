/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/search/SearchParameter.hxx"


SearchParameter::SearchParameter (std::string&& nameUrl, std::string&& nameDb, const Type type)
    : nameUrl(std::move(nameUrl)),
      nameDb(std::move(nameDb)),
      type(type)
{
}

SearchParameter::SearchParameter(std::string&& nameUrl, const SearchParameter::Type type)
    : SearchParameter(std::string(nameUrl), std::move(nameUrl), type)
{
}

SearchParameter::SearchParameter (std::string&& nameUrl, std::string&& nameDb, const Type type,
                                  SearchParameter::SearchToDbValue&& aSearchToDbValue)
    : nameUrl(std::move(nameUrl)),
      nameDb(std::move(nameDb)),
      type(type),
      searchToDbValue(std::in_place, std::move(aSearchToDbValue))
{
    Expect3(type == Type::String, "Type must be String", std::logic_error);
}

bool SearchParameter::operator<(const SearchParameter &other) const
{
    return nameUrl < other.nameUrl;
}
