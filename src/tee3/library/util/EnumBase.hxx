/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_ENUMBASE_HXX
#define EPA_LIBRARY_UTIL_ENUMBASE_HXX

#include "library/util/EnumTable.hxx"

#include <ostream>
#include <string>

namespace epa
{

/**
 * This templated base class uses the Curiously Recurring Template Pattern to provide the following
 * member functions:
 *
 * - Comparison operators: operator==, operator!=, and operator<.
 * - Printing to an ostream: operator<.
 * - JSON input/output if the derived class provides fromJsonString and toJsonString.
 * - A getTable() method that provides conversions from/to strings.
 *
 * It has the following requirements:
 *
 * - The derived class must use EPA_VALUES_ENUM to define "rawNames" and a nested "enum Value".
 * - The derived class must implement a getValue() member function.
 */
template<class Derived>
class EnumBase
{
public:
    static const EnumTable<Derived>& getTable()
    {
        static const EnumTable<Derived> table{Derived::rawNames};
        return table;
    }

    template<typename JsonReader>
    static Derived fromJson(const JsonReader& reader)
    {
        return Derived::fromJsonString(reader.stringValue());
    }

    template<typename JsonWriter>
    friend void writeJson(JsonWriter& writer, Derived value)
    {
        writer.makeStringValue(value.toJsonString());
    }

    friend bool operator==(const Derived& lhs, const Derived& rhs)
    {
        return lhs.getValue() == rhs.getValue();
    }

    friend bool operator!=(const Derived& lhs, const Derived& rhs)
    {
        return lhs.getValue() != rhs.getValue();
    }

    friend bool operator<(const Derived& lhs, const Derived& rhs)
    {
        return lhs.getValue() < rhs.getValue();
    }

    friend std::ostream& operator<<(std::ostream& stream, const Derived& rhs)
    {
        return stream << getTable().toString(rhs.getValue());
    }

protected:
    // Prevent anyone from deleting a derived enum instance through an EnumBase pointer.
    ~EnumBase() = default;
};

} // namespace epa

#endif
