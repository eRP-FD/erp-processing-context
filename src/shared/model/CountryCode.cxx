// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/model/CountryCode.hxx"
#include "shared/util/Expect.hxx"

namespace model
{

CountryCode::CountryCode(const std::string& countryCode)
    : mCountryCode(countryCode)
{
    ModelExpect((mCountryCode.size() == 2), "invalid ISO 3166 ALPHA-2 country code: " + std::string(countryCode));
}

const std::string& CountryCode::toString() const
{
    return mCountryCode;
}

}
