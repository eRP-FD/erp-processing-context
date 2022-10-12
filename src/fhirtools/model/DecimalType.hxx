/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DECIMALTYPE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DECIMALTYPE_HXX

#include <boost/multiprecision/cpp_dec_float.hpp>

namespace boost::multiprecision::detail
{
template<>
struct is_restricted_conversion<double, boost::multiprecision::cpp_dec_float<8>> {
    static constexpr const bool value = true;
};
template<>
struct is_explicitly_convertible<double, boost::multiprecision::cpp_dec_float<8>> {
    static constexpr const bool value = false;
};
template<>
struct is_restricted_conversion<float, boost::multiprecision::cpp_dec_float<8>> {
    static constexpr const bool value = true;
};
template<>
struct is_explicitly_convertible<float, boost::multiprecision::cpp_dec_float<8>> {
    static constexpr const bool value = false;
};
}

namespace fhirtools
{
// A fixed-point number type with precision 10^-8 as specified in http://hl7.org/fhirpath/#decimal
using DecimalType = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<8>>;
}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_DECIMALTYPE_HXX
