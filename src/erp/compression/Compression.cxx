/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/compression/Compression.hxx"
#include "erp/util/Expect.hxx"

#include <magic_enum/magic_enum.hpp>
#include <ostream>
#include <stdexcept>

std::ostream& operator << (std::ostream& out, Compression::DictionaryUse dictUse)
{
    if (magic_enum::enum_contains(dictUse))
    {
        return out << magic_enum::enum_name(dictUse);
    }
    Fail2("Undefined enum value for DictionaryUse: " + std::to_string(uintmax_t(dictUse)), std::logic_error);
}
