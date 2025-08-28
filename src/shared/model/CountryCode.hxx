// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COUNTRYCODE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COUNTRYCODE_HXX

#include <string>

namespace model
{

class CountryCode
{
public:
    explicit CountryCode(const std::string& countryCode);
    const std::string& toString() const;

    [[nodiscard]] bool operator==(const CountryCode& other) const = default;

private:
    std::string mCountryCode;
};

}// model

#endif//ERP_PROCESSING_CONTEXT_MODEL_COUNTRYCODE_HXX
