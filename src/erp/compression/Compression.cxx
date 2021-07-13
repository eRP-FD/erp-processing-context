#include "erp/compression/Compression.hxx"

#include <magic_enum.hpp>
#include <ostream>
#include <stdexcept>

std::ostream& operator << (std::ostream& out, Compression::DictionaryUse dictUse)
{
    if (magic_enum::enum_contains(dictUse))
    {
        return out << magic_enum::enum_name(dictUse);
    }
    throw std::logic_error("Undefined enum value for DictionaryUse: " + std::to_string(uintmax_t(dictUse)));
}
