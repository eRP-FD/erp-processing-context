/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#include <string>
#include <string_view>

namespace util
{

std::string demangle(std::string_view mangledName);

} // namespace util
