/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/PrimitiveValue.hxx"

#include "fhirtools/FPExpect.hxx"

#include <locale>
#include <set>
#include <string_view>
#include <utility>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace fhirtools
{


namespace
{
struct ToString {


    std::string operator()(std::string s) const
    {
        return s;
    }
    std::string operator()(bool b)
    {
        return b ? "true" : "false";
    }
    std::string operator()(int32_t v)
    {
        return std::to_string(v);
    }
    std::string operator()(const DecimalType& dt)
    {
        return to_string(dt);
    }
    template<typename T>
    std::string operator()(T&& v)
        requires std::same_as<T, const Date&> || std::same_as<T, const DateTime&> ||
                 std::same_as<T, const Time&> || std::same_as<T, const QuantityType&>
    {
        using std::to_string;
        return to_string(v);
    }
};

struct ToBool {
    bool operator()(bool v)
    {
        return v;
    }
    bool operator()(int32_t asInt)
    {
        if (asInt == 0)
        {
            return false;
        }
        if (asInt == 1)
        {
            return true;
        }
        FPFail("not convertible to bool");
    }
    bool operator()(const DecimalType& asDecimal)
    {
        if (asDecimal == 0)

        {
            return false;
        }
        if (asDecimal == 1)
        {
            return true;
        }
        FPFail("not convertible to bool");
    }
    bool operator()(const std::string& asString)
    {
        struct ICompare {
            bool operator() (const std::string& lhs, const std::string& rhs) const
            {
                return boost::ilexicographical_compare(lhs, rhs, std::locale::classic());
            };
        };
        static const std::set<std::string, ICompare> trueValues{"true", "t", "yes", "y", "1", "1.0"};
        static const std::set<std::string, ICompare> falseValues{"false", "f", "no", "n", "0", "0.0"};
        if (trueValues.contains(asString))
        {
            return true;
        }
        if (falseValues.contains(asString))
        {
            return false;
        }
        FPFail("not convertible to bool");
    }
    template <typename T>
    bool operator()(const T&)
    requires std::same_as<T, Date> || std::same_as<T, DateTime> || std::same_as<T, Time> || std::same_as<T, QuantityType>
    {
        FPFail("not convertible to bool");
    }
};

struct ToDate {
    Date operator() (const Date& d)
    {
        return d;
    }
    Date operator() (const DateTime& dateTime)
    {
        return dateTime.date();
    }
    Date operator() (const std::string& str)
    {
        return Date(str);
    }
    template <typename T>
    Date operator()(const T&)
        requires std::same_as<T, Time> || std::same_as<T, QuantityType> || std::same_as<T, int32_t> ||
                 std::same_as<T, bool> || std::same_as<T, DecimalType>
    {
        FPFail("not convertible to Date");
    }
};

struct ToDateTime {
    DateTime operator() (const Date& date)
    {
        return DateTime{date};
    }
    DateTime operator() (const DateTime& dateTime)
    {
        return dateTime;
    }
    DateTime operator() (const std::string& str)
    {
        return DateTime(str);
    }
    template <typename T>
    DateTime operator()(const T&)
    requires std::same_as<T, Time> || std::same_as<T, QuantityType> || std::same_as<T, int32_t> || std::same_as<T, bool> || std::same_as<T, DecimalType>
    {
        FPFail("not convertible to DateTime");
    }
};

struct ToTime {
    Time operator()(const Time& time) const
    {
        return time;
    }
    Time operator()(const std::string& str) const
    {
        return Time(str);
    }
    template <typename T>
    Time operator()(const T&) const
        requires std::same_as<T, Date> || std::same_as<T, DateTime> || std::same_as<T, QuantityType> ||
                 std::same_as<T, int32_t> || std::same_as<T, bool> || std::same_as<T, DecimalType>
    {
        FPFail("not convertible to Time");
    }
};

struct ToDecimal {
    DecimalType operator()(const DecimalType& decimal) const
    {
        return decimal;
    }
    DecimalType operator()(int32_t v) const
    {
        return v;
    }
    DecimalType operator()(bool v) const
    {
        return v ? DecimalType("1.0") : DecimalType("0.0");
    }
    DecimalType operator()(const std::string& str) const
    {
        return DecimalType(str);
    }
    template <typename T>
    DecimalType operator()(const T&) const
        requires std::same_as<T, Date> || std::same_as<T, DateTime> || std::same_as<T, Time> ||
                 std::same_as<T, QuantityType>
    {
        FPFail("not convertible to decimal");
    }
};

struct ToInt {
    int32_t operator()(int32_t v) const
    {
        return v;
    }
    int32_t operator()(bool v) const
    {
        return v ? 1 : 0;
    }
    int32_t operator()(const std::string& asString) const
    {
        size_t idx = 0;
        auto asInt = std::stoi(asString, &idx);
        if (idx == asString.size())
        {
            return asInt;
        }
        FPFail("not convertible to int");
    }
    template <typename T>
    int32_t operator()(const T&) const
        requires std::same_as<T, DecimalType> || std::same_as<T, Date> || std::same_as<T, DateTime> ||
                 std::same_as<T, Time> || std::same_as<T, QuantityType>
    {
        FPFail("not convertible to int");
    }
};

struct ToQuantity {
    QuantityType operator()(int32_t v) const
    {
        return {v, ""};
    }
    QuantityType operator()(const DecimalType& v) const
    {
        return {v, ""};
    }
    QuantityType operator()(const std::string& str) const
    {
        return QuantityType(str);
    }
    QuantityType operator()(const QuantityType& q) const
    {
        return q;
    }
    template <typename T>
    QuantityType operator()(const T&) const
        requires std::same_as<T, bool> || std::same_as<T, Date> || std::same_as<T, DateTime> || std::same_as<T, Time>
    {
        FPFail("not convertible to Quantity");
    }
};

}

QuantityType::QuantityType(DecimalType value, const std::string_view& unit)
    : mValue(std::move(value))
    , mUnit(unit)
{
}

QuantityType::QuantityType(const std::string_view& valueAndUnit)
{
    size_t idx = 0;
    (void) std::stod(std::string(valueAndUnit), &idx);
    mValue = DecimalType(std::string_view{valueAndUnit.begin(), idx});
    mUnit = boost::trim_copy(std::string(valueAndUnit.substr(idx)));
}

const DecimalType& fhirtools::QuantityType::value() const
{
    return mValue;
}

const std::string& fhirtools::QuantityType::unit() const
{
    return mUnit;
}

std::optional<std::strong_ordering> QuantityType::compareTo(const QuantityType& rhs) const
{
    if (! convertibleToUnit(rhs.mUnit))
    {
        return std::nullopt;
    }
    const auto convertedUnit = convertToUnit(rhs.mUnit);
    if (convertedUnit.mValue < rhs.mValue)
    {
        return std::strong_ordering::less;
    }
    if (convertedUnit.mValue > rhs.mValue)
    {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

std::optional<bool> QuantityType::equals(const QuantityType& rhs) const
{
    const auto result = compareTo(rhs);
    if (! result.has_value())
    {
        return std::nullopt;
    }
    return result == std::strong_ordering::equal;
}

QuantityType QuantityType::convertToUnit(const std::string& unit) const
{
    FPExpect(mUnit == unit, "unit " + mUnit + " not convertible to " + unit);
    return *this;
}

bool QuantityType::convertibleToUnit(const std::string& unit) const
{
    return mUnit == unit;
}

std::string to_string(const QuantityType& q)
{
    return to_string(q.value()) + " " + q.unit();
}

bool fhirtools::PrimitiveValue::asBool() const
{
    return std::visit(ToBool{}, *this);
}

Date fhirtools::PrimitiveValue::asDate() const
{
    return std::visit(ToDate{}, *this);
}

DateTime fhirtools::PrimitiveValue::asDateTime() const
{
    return std::visit(ToDateTime{}, *this);
}

Time fhirtools::PrimitiveValue::asTime() const
{
    return std::visit(ToTime{}, *this);
}

DecimalType fhirtools::PrimitiveValue::asDecimal() const
{
    return std::visit(ToDecimal{}, *this);
}

int32_t fhirtools::PrimitiveValue::asInt() const
{
    return std::visit(ToInt{}, *this);
}

QuantityType fhirtools::PrimitiveValue::asQuantity() const
{
    return std::visit(ToQuantity{}, *this);
}

std::string fhirtools::PrimitiveValue::asString() const
{
    return std::visit(ToString{}, *this);
}

}
